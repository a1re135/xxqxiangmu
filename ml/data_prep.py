"""UC-M-01 training-data preparation for the NCS charging platform."""

from __future__ import annotations

import csv
import json
import os
import sqlite3
import sys
from collections import defaultdict
from datetime import date, datetime, timedelta
from pathlib import Path
from typing import Iterable

from .holidays import is_public_holiday
from .weather import simulated_weather


REQUIRED_ORDER_COLUMNS = {
    "charger_id", "start_time", "energy", "status"
}

OUTPUT_DATASET = Path("ml/data/charging_load_training.csv")
OUTPUT_HOURLY = Path("ml/data/hourly_aggregated.csv")
OUTPUT_ENCODER = Path("ml/data/station_encoder.json")
OUTPUT_METADATA = Path("ml/data/dataset_metadata.json")

FEATURE_COLUMNS = [
    "hour",
    "weekday",
    "is_weekend",
    "is_holiday",
    "lag_1h",
    "lag_24h",
    "lag_168h",
    "rolling_mean_24h",
    "station_encoding",
    "temperature_c",
    "is_precipitation",
]

TARGET_COLUMN = "charge_load_kwh"


class DataPreparationError(RuntimeError):
    """Raised when the source database cannot produce a valid training set."""


def default_database_path() -> Path:
    """Resolve the same conventional GenericDataLocation layout as the Qt app."""
    if os.name == "nt":
        base = Path(os.environ.get("LOCALAPPDATA", Path.home() / "AppData/Local"))
    elif sys.platform == "darwin":
        base = Path.home() / "Library/Application Support"
    else:
        base = Path(os.environ.get("XDG_DATA_HOME", Path.home() / ".local/share"))
    return base / "NCS_Charging_Platform" / "charge_platform.db"


def _parse_timestamp(value: str, row_id: int) -> datetime:
    text = (value or "").strip()
    if not text:
        raise DataPreparationError(
            f"charging_order.id={row_id} has an empty start_time; cannot aggregate it."
        )

    for fmt in ("%Y-%m-%d %H:%M:%S", "%Y-%m-%d %H:%M", "%Y-%m-%dT%H:%M:%S"):
        try:
            return datetime.strptime(text, fmt)
        except ValueError:
            pass
    raise DataPreparationError(
        f"charging_order.id={row_id} has unsupported start_time '{text}'."
    )


def _check_schema(conn: sqlite3.Connection) -> None:
    rows = conn.execute(
        "PRAGMA table_info(charging_order)"
    ).fetchall()
    columns = {row[1] for row in rows}
    missing = sorted(REQUIRED_ORDER_COLUMNS - columns)
    if missing:
        raise DataPreparationError(
            "charging_order is missing required columns: " + ", ".join(missing)
        )

    for table in ("charging_order", "charger", "station"):
        exists = conn.execute(
            "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?",
            (table,),
        ).fetchone()
        if not exists:
            raise DataPreparationError(f"Required table '{table}' does not exist.")


def _load_station_encoder(conn: sqlite3.Connection) -> dict[int, int]:
    rows = conn.execute("SELECT id FROM station ORDER BY id").fetchall()
    if not rows:
        raise DataPreparationError("No stations exist in the database.")
    return {int(row[0]): index for index, row in enumerate(rows)}


def _fetch_completed_orders(
    conn: sqlite3.Connection,
) -> Iterable[tuple[int, datetime, float]]:
    query = """
        SELECT o.id, c.station_id, o.start_time, o.energy
        FROM charging_order AS o
        INNER JOIN charger AS c ON c.id = o.charger_id
        WHERE o.status = 2
          AND o.start_time IS NOT NULL
        ORDER BY c.station_id, o.start_time, o.id
    """
    rows = conn.execute(query)
    count = 0
    for row_id, station_id, start_time, energy in rows:
        timestamp = _parse_timestamp(str(start_time), int(row_id))
        try:
            station_id = int(station_id)
            energy = float(energy)
        except (TypeError, ValueError) as exc:
            raise DataPreparationError(
                f"charging_order.id={row_id} has invalid station/energy data."
            ) from exc
        if energy < 0:
            raise DataPreparationError(
                f"charging_order.id={row_id} has negative energy={energy}."
            )
        yield station_id, timestamp.replace(minute=0, second=0, microsecond=0), energy
        count += 1
    if count == 0:
        raise DataPreparationError(
            "No completed charging orders (status=2) with start_time were found."
        )


def _aggregate_orders(
    records: Iterable[tuple[int, datetime, float]],
    station_encoder: dict[int, int],
) -> dict[int, dict[datetime, float]]:
    aggregated: dict[int, dict[datetime, float]] = defaultdict(lambda: defaultdict(float))
    for station_id, hour_bucket, energy in records:
        if station_id not in station_encoder:
            raise DataPreparationError(
                f"charging_order references unknown station_id={station_id}."
            )
        aggregated[station_id][hour_bucket] += energy
    return aggregated


def _continuous_hourly_rows(
    aggregated: dict[int, dict[datetime, float]],
) -> list[dict[str, object]]:
    """Fill absent station-hours with zero load so lags remain true hourly lags."""
    rows: list[dict[str, object]] = []
    for station_id in sorted(aggregated):
        observed = aggregated[station_id]
        start = min(observed)
        end = max(observed)
        cursor = start
        while cursor <= end:
            rows.append(
                {
                    "station_id": station_id,
                    "timestamp": cursor,
                    TARGET_COLUMN: round(observed.get(cursor, 0.0), 6),
                }
            )
            cursor += timedelta(hours=1)
    return rows


def _add_features(rows: list[dict[str, object]], station_encoder: dict[int, int]) -> list[dict[str, object]]:
    by_station: dict[int, list[dict[str, object]]] = defaultdict(list)
    for row in rows:
        by_station[int(row["station_id"])].append(row)

    featured: list[dict[str, object]] = []
    for station_id, station_rows in by_station.items():
        station_rows.sort(key=lambda item: item["timestamp"])
        loads = [float(item[TARGET_COLUMN]) for item in station_rows]

        for index, row in enumerate(station_rows):
            timestamp = row["timestamp"]
            assert isinstance(timestamp, datetime)
            if index < 168:
                continue

            previous_24 = loads[index - 24:index]
            if len(previous_24) != 24:
                continue

            temperature, precipitation = simulated_weather(station_id, timestamp)
            output_row = dict(row)
            output_row.update(
                {
                    "date": timestamp.date().isoformat(),
                    "timestamp": timestamp.strftime("%Y-%m-%d %H:%M:%S"),
                    "hour": timestamp.hour,
                    # Python Monday=0 ... Sunday=6.
                    "weekday": timestamp.weekday(),
                    "is_weekend": int(timestamp.weekday() >= 5),
                    "is_holiday": int(is_public_holiday(timestamp.date())),
                    "lag_1h": round(loads[index - 1], 6),
                    "lag_24h": round(loads[index - 24], 6),
                    "lag_168h": round(loads[index - 168], 6),
                    "rolling_mean_24h": round(sum(previous_24) / 24.0, 6),
                    "station_encoding": station_encoder[station_id],
                    "temperature_c": temperature,
                    "is_precipitation": precipitation,
                    TARGET_COLUMN: round(loads[index], 6),
                }
            )
            featured.append(output_row)
    return featured


def _write_csv(path: Path, rows: list[dict[str, object]], fieldnames: list[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def prepare_training_data(
    db_path: Path,
    output_root: Path | None = None,
) -> dict[str, object]:
    """Build the UC-M-01 hourly feature dataset from the SQLite database."""
    db_path = Path(db_path).expanduser().resolve()
    if not db_path.is_file():
        raise DataPreparationError(f"Database file does not exist: {db_path}")

    root = (output_root or Path.cwd()).resolve()
    dataset_path = root / OUTPUT_DATASET
    hourly_path = root / OUTPUT_HOURLY
    encoder_path = root / OUTPUT_ENCODER
    metadata_path = root / OUTPUT_METADATA

    try:
        conn = sqlite3.connect(f"file:{db_path.as_posix()}?mode=ro", uri=True)
    except sqlite3.Error as exc:
        raise DataPreparationError(f"Unable to open SQLite database: {exc}") from exc

    try:
        _check_schema(conn)
        encoder = _load_station_encoder(conn)
        records = list(_fetch_completed_orders(conn))
        aggregated = _aggregate_orders(records, encoder)
        hourly_rows = _continuous_hourly_rows(aggregated)
        training_rows = _add_features(hourly_rows, encoder)
    finally:
        conn.close()

    if not training_rows:
        raise DataPreparationError(
            "The database does not contain enough continuous hourly history "
            "to create lag_168h and rolling_mean_24h features."
        )

    hourly_fields = ["station_id", "timestamp", TARGET_COLUMN]
    training_fields = [
        "station_id",
        "date",
        "timestamp",
        *FEATURE_COLUMNS,
        TARGET_COLUMN,
    ]
    _write_csv(hourly_path, hourly_rows, hourly_fields)
    _write_csv(dataset_path, training_rows, training_fields)

    encoder_payload = {
        str(station_id): encoding for station_id, encoding in encoder.items()
    }
    encoder_path.parent.mkdir(parents=True, exist_ok=True)
    encoder_path.write_text(
        json.dumps(encoder_payload, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )

    all_timestamps = [row["timestamp"] for row in hourly_rows]
    metadata = {
        "stage": "UC-M-01",
        "source_database": str(db_path),
        "source_status_filter": 2,
        "aggregation": "station + calendar date + hour",
        "time_bucket": "start_time truncated to hour",
        "missing_hours_filled_with_zero": True,
        "lag_features_hours": [1, 24, 168],
        "rolling_mean_window_hours": 24,
        "rolling_mean_excludes_current_target": True,
        "station_encoding": "station id ordered ascending, encoded from 0",
        "weather": "deterministic simulated temperature and precipitation flag",
        "target": TARGET_COLUMN,
        "station_count": len(encoder),
        "completed_order_count": len(records),
        "hourly_row_count": len(hourly_rows),
        "training_row_count": len(training_rows),
        "time_range_start": min(all_timestamps).strftime("%Y-%m-%d %H:%M:%S"),
        "time_range_end": max(all_timestamps).strftime("%Y-%m-%d %H:%M:%S"),
        "feature_columns": FEATURE_COLUMNS,
        "target_column": TARGET_COLUMN,
        "outputs": {
            "training_csv": str(dataset_path),
            "hourly_csv": str(hourly_path),
            "station_encoder_json": str(encoder_path),
        },
    }
    metadata_path.write_text(
        json.dumps(metadata, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )
    return metadata
