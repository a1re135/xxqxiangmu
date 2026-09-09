from __future__ import annotations

import argparse
import sqlite3
from pathlib import Path

import numpy as np
import pandas as pd

from common import (
    ensure_database_exists,
    resolve_database_path,
)


RANDOM_SEED = 42


def load_orders(db_path: Path) -> pd.DataFrame:
    connection = sqlite3.connect(db_path)

    try:
        query = """
        SELECT
            o.id AS order_id,
            o.start_time,
            o.end_time,
            o.energy,
            o.amount,
            o.status,

            c.id AS charger_id,
            c.station_id,
            c.type AS charger_type,
            c.power AS charger_power

        FROM charging_order AS o

        JOIN charger AS c
          ON c.id = o.charger_id

        WHERE o.status = 2
          AND o.start_time IS NOT NULL
          AND o.energy >= 0

        ORDER BY o.start_time
        """

        frame = pd.read_sql_query(
            query,
            connection,
        )

    finally:
        connection.close()

    return frame


def build_hourly_dataset(
    orders: pd.DataFrame,
) -> pd.DataFrame:

    if orders.empty:
        raise ValueError(
            "No completed charging orders were found."
        )

    orders = orders.copy()

    orders["start_time"] = pd.to_datetime(
        orders["start_time"],
        errors="coerce",
    )

    orders = orders.dropna(
        subset=["start_time"]
    )

    if orders.empty:
        raise ValueError(
            "Completed orders contain no valid start times."
        )

    # Round every charging session down to its starting hour.
    orders["timestamp"] = (
        orders["start_time"]
        .dt.floor("h")
    )

    # Hourly energy load for each station.
    hourly = (
        orders.groupby(
            ["station_id", "timestamp"],
            as_index=False,
        )
        .agg(
            load_kwh=("energy", "sum"),
            order_count=("order_id", "count"),
        )
    )

    completed_frames = []

    for station_id, group in hourly.groupby(
        "station_id"
    ):
        group = group.sort_values(
            "timestamp"
        ).copy()

        start_time = (
            group["timestamp"]
            .min()
            .floor("D")
        )

        end_time = (
	    group["timestamp"]
	    .max()
	    .floor("D")
	    + pd.Timedelta(hours=23)
	)

        complete_index = pd.date_range(
            start=start_time,
            end=end_time,
            freq="h",
        )

        station_frame = (
            group.set_index("timestamp")
            .reindex(complete_index)
        )

        station_frame.index.name = "timestamp"

        station_frame["station_id"] = station_id

        station_frame["load_kwh"] = (
            station_frame["load_kwh"]
            .fillna(0.0)
        )

        station_frame["order_count"] = (
            station_frame["order_count"]
            .fillna(0)
            .astype(int)
        )

        station_frame = (
            station_frame
            .reset_index()
        )

        completed_frames.append(
            station_frame
        )

    dataset = pd.concat(
        completed_frames,
        ignore_index=True,
    )

    dataset = dataset.sort_values(
        ["station_id", "timestamp"]
    ).reset_index(drop=True)

    return dataset


def add_calendar_features(
    dataset: pd.DataFrame,
) -> pd.DataFrame:

    frame = dataset.copy()

    timestamp = frame["timestamp"]

    frame["hour"] = timestamp.dt.hour
    frame["day_of_week"] = timestamp.dt.dayofweek
    frame["day_of_month"] = timestamp.dt.day
    frame["month"] = timestamp.dt.month

    frame["is_weekend"] = (
        frame["day_of_week"] >= 5
    ).astype(int)
    
    frame["is_holiday"] = (
    frame["timestamp"]
    .apply(is_holiday)
    .astype(int)
    )

    # Cyclic hour encoding.
    frame["hour_sin"] = np.sin(
        2.0
        * np.pi
        * frame["hour"]
        / 24.0
    )

    frame["hour_cos"] = np.cos(
        2.0
        * np.pi
        * frame["hour"]
        / 24.0
    )

    # Cyclic weekday encoding.
    frame["weekday_sin"] = np.sin(
        2.0
        * np.pi
        * frame["day_of_week"]
        / 7.0
    )

    frame["weekday_cos"] = np.cos(
        2.0
        * np.pi
        * frame["day_of_week"]
        / 7.0
    )

    return frame


def add_lag_features(
    dataset: pd.DataFrame,
) -> pd.DataFrame:

    frame = dataset.copy()

    grouped = frame.groupby(
        "station_id",
        group_keys=False,
    )

    # Previous-hour demand.
    frame["load_lag_1h"] = (
        grouped["load_kwh"]
        .shift(1)
    )

    # Same hour yesterday.
    frame["load_lag_24h"] = (
        grouped["load_kwh"]
        .shift(24)
    )

    # Same hour one week ago.
    frame["load_lag_168h"] = (
        grouped["load_kwh"]
        .shift(168)
    )

    # Moving averages use only previous values,
    # avoiding target leakage.
    frame["load_avg_6h"] = (
        grouped["load_kwh"]
        .transform(
            lambda s:
                s.shift(1)
                .rolling(
                    window=6,
                    min_periods=1,
                )
                .mean()
        )
    )

    frame["load_avg_24h"] = (
        grouped["load_kwh"]
        .transform(
            lambda s:
                s.shift(1)
                .rolling(
                    window=24,
                    min_periods=1,
                )
                .mean()
        )
    )

    frame["orders_lag_1h"] = (
        grouped["order_count"]
        .shift(1)
    )

    return frame


def add_demo_weather_features(
    dataset: pd.DataFrame,
) -> pd.DataFrame:
    """
    Deterministic weather-like features.

    This avoids depending on an internet weather service
    during your semester demo while still providing the
    required weather-related model inputs.

    These can later be replaced with a real weather API.
    """

    frame = dataset.copy()

    hour = frame["hour"].astype(float)

    # Daily temperature pattern.
    frame["temperature_c"] = (
        20.0
        + 7.0
        * np.sin(
            2.0
            * np.pi
            * (hour - 8.0)
            / 24.0
        )
    )

    # Deterministic pseudo-humidity.
    frame["humidity"] = (
        60.0
        - 12.0
        * np.sin(
            2.0
            * np.pi
            * (hour - 8.0)
            / 24.0
        )
    )

    # Deterministic rain indicator.
    frame["is_rain"] = (
        (
            frame["timestamp"].dt.day
            + frame["station_id"]
        )
        % 9
        == 0
    ).astype(int)

    return frame


def build_training_dataset(
    db_path: Path,
) -> pd.DataFrame:

    orders = load_orders(db_path)

    print(
        f"Loaded {len(orders)} completed orders."
    )

    frame = build_hourly_dataset(orders)
    frame = add_calendar_features(frame)
    frame = add_lag_features(frame)
    frame = add_demo_weather_features(frame)

    # Remove rows without sufficient lag history.
    frame = frame.dropna(
        subset=[
            "load_lag_1h",
            "load_lag_24h",
            "load_lag_168h",
            "orders_lag_1h",
        ]
    )

    frame = frame.reset_index(drop=True)

    return frame


HOLIDAY_MONTH_DAYS = {
    (1, 1),   # New Year
    (5, 1),   # Labour Day

    (10, 1),
    (10, 2),
    (10, 3),
    (10, 4),
    (10, 5),
    (10, 6),
    (10, 7),
}


def is_holiday(
    timestamp: pd.Timestamp,
) -> int:
    """
    Deterministic holiday feature used by the
    demo ML pipeline.

    1 = holiday
    0 = normal day
    """

    return int(
        (
            timestamp.month,
            timestamp.day,
        )
        in HOLIDAY_MONTH_DAYS
    )


def main():
    parser = argparse.ArgumentParser(
        description=(
            "Build hourly charging-load "
            "training data."
        )
    )

    parser.add_argument(
        "--db",
        default=None,
        help="Optional charge_platform.db path",
    )

    parser.add_argument(
        "--output",
        default="ml/training_data.csv",
        help="Output CSV path",
    )

    args = parser.parse_args()

    np.random.seed(RANDOM_SEED)

    db_path = resolve_database_path(
        args.db
    )

    ensure_database_exists(db_path)

    print(f"Using database: {db_path}")

    dataset = build_training_dataset(
        db_path
    )

    output_path = Path(
        args.output
    )

    output_path.parent.mkdir(
        parents=True,
        exist_ok=True,
    )

    dataset.to_csv(
        output_path,
        index=False,
    )

    print()
    print("Training data created successfully.")
    print(f"Rows:    {len(dataset)}")
    print(f"Columns: {len(dataset.columns)}")
    print(f"Output:  {output_path}")

    if not dataset.empty:
        print()
        print("Time range:")
        print(
            dataset["timestamp"].min(),
            "->",
            dataset["timestamp"].max(),
        )

        print()
        print("Stations:")
        print(
            sorted(
                dataset["station_id"]
                .unique()
                .tolist()
            )
        )


if __name__ == "__main__":
    main()
