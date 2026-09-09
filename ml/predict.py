from __future__ import annotations

import argparse
import math
import sqlite3
from pathlib import Path

import joblib
import numpy as np
import pandas as pd

from common import (
    ensure_database_exists,
    resolve_database_path,
)

from prepare_data import (
    build_hourly_dataset,
    load_orders,
)


RANDOM_SEED = 42


def load_model(model_path: Path):
    if not model_path.exists():
        raise FileNotFoundError(
            f"Model not found: {model_path}\n"
            "Run ml/train_model.py first."
        )

    package = joblib.load(model_path)

    if "pipeline" not in package:
        raise ValueError(
            "Invalid model package: pipeline is missing."
        )

    return package


def load_station_info(
    db_path: Path,
    station_id: int | None,
):
    connection = sqlite3.connect(db_path)

    try:
        if station_id is None:
            query = """
            SELECT
                s.id,
                s.name,
                COUNT(c.id) AS charger_count
            FROM station s
            LEFT JOIN charger c
              ON c.station_id = s.id
            GROUP BY s.id, s.name
            ORDER BY s.id
            """

            rows = connection.execute(
                query
            ).fetchall()

        else:
            query = """
            SELECT
                s.id,
                s.name,
                COUNT(c.id) AS charger_count
            FROM station s
            LEFT JOIN charger c
              ON c.station_id = s.id
            WHERE s.id = ?
            GROUP BY s.id, s.name
            """

            rows = connection.execute(
                query,
                (station_id,),
            ).fetchall()

    finally:
        connection.close()

    return rows


def average_energy_per_order(
    orders: pd.DataFrame,
    station_id: int,
) -> float:
    station_orders = orders[
        orders["station_id"] == station_id
    ]

    positive = station_orders[
        station_orders["energy"] > 0
    ]

    if positive.empty:
        return 20.0

    value = float(
        positive["energy"].mean()
    )

    return max(value, 1.0)


def weather_features(timestamp: pd.Timestamp):
    """
    Same deterministic weather feature logic used
    by prepare_data.py.
    """

    hour = float(timestamp.hour)

    temperature = (
        20.0
        + 7.0
        * np.sin(
            2.0
            * np.pi
            * (hour - 8.0)
            / 24.0
        )
    )

    humidity = (
        60.0
        - 12.0
        * np.sin(
            2.0
            * np.pi
            * (hour - 8.0)
            / 24.0
        )
    )

    return (
        float(temperature),
        float(humidity),
    )


def historical_value(
    values: dict[pd.Timestamp, float],
    timestamp: pd.Timestamp,
) -> float:
    return float(
        values.get(timestamp, 0.0)
    )


def rolling_average(
    values: dict[pd.Timestamp, float],
    target_time: pd.Timestamp,
    hours: int,
) -> float:
    samples = []

    for offset in range(1, hours + 1):
        timestamp = (
            target_time
            - pd.Timedelta(hours=offset)
        )

        samples.append(
            historical_value(
                values,
                timestamp,
            )
        )

    if not samples:
        return 0.0

    return float(
        np.mean(samples)
    )


def create_feature_row(
    station_id: int,
    target_time: pd.Timestamp,
    load_history: dict[pd.Timestamp, float],
    order_history: dict[pd.Timestamp, float],
):
    day_of_week = target_time.dayofweek
    hour = target_time.hour

    temperature, humidity = (
        weather_features(target_time)
    )

    is_rain = int(
        (
            target_time.day
            + station_id
        )
        % 9
        == 0
    )

    return {
        "hour": hour,
        "day_of_week": day_of_week,
        "day_of_month": target_time.day,
        "month": target_time.month,

        "is_weekend":
            int(day_of_week >= 5),

        "hour_sin":
            np.sin(
                2.0
                * np.pi
                * hour
                / 24.0
            ),

        "hour_cos":
            np.cos(
                2.0
                * np.pi
                * hour
                / 24.0
            ),

        "weekday_sin":
            np.sin(
                2.0
                * np.pi
                * day_of_week
                / 7.0
            ),

        "weekday_cos":
            np.cos(
                2.0
                * np.pi
                * day_of_week
                / 7.0
            ),

        "load_lag_1h":
            historical_value(
                load_history,
                target_time
                - pd.Timedelta(hours=1),
            ),

        "load_lag_24h":
            historical_value(
                load_history,
                target_time
                - pd.Timedelta(hours=24),
            ),

        "load_lag_168h":
            historical_value(
                load_history,
                target_time
                - pd.Timedelta(hours=168),
            ),

        "load_avg_6h":
            rolling_average(
                load_history,
                target_time,
                6,
            ),

        "load_avg_24h":
            rolling_average(
                load_history,
                target_time,
                24,
            ),

        "orders_lag_1h":
            historical_value(
                order_history,
                target_time
                - pd.Timedelta(hours=1),
            ),

        "temperature_c":
            temperature,

        "humidity":
            humidity,

        "is_rain":
            is_rain,

        "station_id":
            station_id,
    }


def prepare_station_history(
    hourly: pd.DataFrame,
    station_id: int,
    base_time: pd.Timestamp,
):
    station = hourly[
        hourly["station_id"] == station_id
    ].copy()

    if station.empty:
        raise ValueError(
            f"Station {station_id} has no historical data."
        )

    station["timestamp"] = pd.to_datetime(
        station["timestamp"]
    )

    # Don't use future-filled rows if the current
    # day's history extends beyond the current hour.
    station = station[
        station["timestamp"] <= base_time
    ]

    load_history = {
        pd.Timestamp(row.timestamp):
            float(row.load_kwh)

        for row in station.itertuples()
    }

    order_history = {
        pd.Timestamp(row.timestamp):
            float(row.order_count)

        for row in station.itertuples()
    }

    return (
        load_history,
        order_history,
    )


def predict_station(
    pipeline,
    hourly: pd.DataFrame,
    orders: pd.DataFrame,
    station_id: int,
    station_name: str,
    charger_count: int,
    horizon: int,
):
    # Start forecasting from the next complete hour.
    base_time = pd.Timestamp.now().floor("h")

    (
        load_history,
        order_history,
    ) = prepare_station_history(
        hourly,
        station_id,
        base_time,
    )

    avg_energy = average_energy_per_order(
        orders,
        station_id,
    )

    results = []

    for step in range(1, horizon + 1):
        target_time = (
            base_time
            + pd.Timedelta(hours=step)
        )

        features = create_feature_row(
            station_id,
            target_time,
            load_history,
            order_history,
        )

        input_frame = pd.DataFrame(
            [features]
        )

        predicted_load = float(
            pipeline.predict(
                input_frame
            )[0]
        )

        # Charging load cannot be negative.
        predicted_load = max(
            0.0,
            predicted_load,
        )

        predicted_load = round(
            predicted_load,
            4,
        )

        # Estimate the number of charging sessions
        # represented by this predicted hourly load.
        if predicted_load <= 0.05:
            estimated_busy = 0

        else:
            estimated_busy = max(
                1,
                int(
                    math.ceil(
                        predicted_load
                        / avg_energy
                    )
                ),
            )

        estimated_busy = min(
            estimated_busy,
            charger_count,
        )

        predicted_free = max(
            0,
            charger_count
            - estimated_busy,
        )

        results.append({
            "station_id":
                station_id,

            "station_name":
                station_name,

            "target_time":
                target_time,

            "predicted_load":
                predicted_load,

            "predicted_free_chargers":
                predicted_free,

            "is_peak":
                0,
        })

        # Recursive forecasting:
        # this prediction becomes historical input
        # for the following forecast hour.
        load_history[
            target_time
        ] = predicted_load

        estimated_orders = (
            0
            if predicted_load <= 0.05
            else max(
                1,
                int(
                    round(
                        predicted_load
                        / avg_energy
                    )
                ),
            )
        )

        order_history[
            target_time
        ] = estimated_orders

    # ------------------------------------------
    # Mark peak hours
    # ------------------------------------------

    if results:
        loads = [
            row["predicted_load"]
            for row in results
        ]

        average_load = float(
            np.mean(loads)
        )

        max_index = int(
            np.argmax(loads)
        )

        for index, row in enumerate(results):
            is_peak = (
                row["predicted_load"]
                >= average_load * 1.20
                and row["predicted_load"] > 0
            )

            if index == max_index:
                is_peak = True

            row["is_peak"] = int(
                is_peak
            )

    return results


def save_predictions(
    db_path: Path,
    predictions: list[dict],
    generated_time: str,
):
    connection = sqlite3.connect(db_path)

    try:
        connection.execute(
            "PRAGMA foreign_keys = ON"
        )

        cursor = connection.cursor()

        query = """
        INSERT INTO load_prediction (
            station_id,
            generated_time,
            target_time,
            predicted_load,
            predicted_free_chargers,
            is_peak
        )
        VALUES (?, ?, ?, ?, ?, ?)
        """

        for row in predictions:
            cursor.execute(
                query,
                (
                    row["station_id"],
                    generated_time,

                    row["target_time"].strftime(
                        "%Y-%m-%d %H:%M:%S"
                    ),

                    row["predicted_load"],
                    row[
                        "predicted_free_chargers"
                    ],
                    row["is_peak"],
                ),
            )

        connection.commit()

    except Exception:
        connection.rollback()
        raise

    finally:
        connection.close()


def print_predictions(
    predictions: list[dict],
):
    if not predictions:
        return

    print()
    print("=" * 80)

    current_station = None

    for row in predictions:
        if (
            row["station_id"]
            != current_station
        ):
            current_station = (
                row["station_id"]
            )

            print()
            print(
                f"Station {row['station_id']}: "
                f"{row['station_name']}"
            )

            print(
                "-" * 80
            )

        peak_text = (
            "PEAK"
            if row["is_peak"]
            else ""
        )

        print(
            row["target_time"].strftime(
                "%Y-%m-%d %H:%M"
            ),
            f"load={row['predicted_load']:.4f} kWh",
            f"free={row['predicted_free_chargers']}",
            peak_text,
        )

    print()
    print("=" * 80)


def main():
    parser = argparse.ArgumentParser(
        description=(
            "Predict future charging load "
            "using the trained RandomForest model."
        )
    )

    parser.add_argument(
        "--db",
        default=None,
        help="Optional charge_platform.db path",
    )

    parser.add_argument(
        "--model",
        default="ml/models/load_rf.pkl",
        help="Trained model path",
    )

    parser.add_argument(
        "--horizon",
        type=int,
        choices=[1, 6, 24],
        default=24,
        help=(
            "Prediction horizon in hours: "
            "1, 6 or 24"
        ),
    )

    parser.add_argument(
        "--station",
        type=int,
        default=None,
        help=(
            "Optional station ID. "
            "If omitted, predict all stations."
        ),
    )

    parser.add_argument(
        "--no-write",
        action="store_true",
        help=(
            "Run prediction without writing "
            "results to SQLite."
        ),
    )

    args = parser.parse_args()

    np.random.seed(
        RANDOM_SEED
    )

    db_path = resolve_database_path(
        args.db
    )

    ensure_database_exists(
        db_path
    )

    model_path = Path(
        args.model
    )

    package = load_model(
        model_path
    )

    pipeline = package[
        "pipeline"
    ]

    print(
        f"Database: {db_path}"
    )

    print(
        f"Model:    {model_path}"
    )

    print(
        f"Horizon:  {args.horizon} hour(s)"
    )


    # ==========================================
    # Historical data
    # ==========================================

    orders = load_orders(
        db_path
    )

    if orders.empty:
        raise RuntimeError(
            "No completed charging orders "
            "are available."
        )

    hourly = build_hourly_dataset(
        orders
    )


    # ==========================================
    # Stations
    # ==========================================

    stations = load_station_info(
        db_path,
        args.station,
    )

    if not stations:
        raise RuntimeError(
            "No matching charging station found."
        )


    # ==========================================
    # Predict
    # ==========================================

    all_predictions = []

    for (
        station_id,
        station_name,
        charger_count,
    ) in stations:

        predictions = predict_station(
            pipeline=pipeline,
            hourly=hourly,
            orders=orders,

            station_id=station_id,
            station_name=station_name,

            charger_count=charger_count,

            horizon=args.horizon,
        )

        all_predictions.extend(
            predictions
        )


    print_predictions(
        all_predictions
    )


    # ==========================================
    # Write to SQLite
    # ==========================================

    if not args.no_write:
        generated_time = (
            pd.Timestamp.now()
            .strftime(
                "%Y-%m-%d %H:%M:%S.%f"
            )[:-3]
        )

        save_predictions(
            db_path,
            all_predictions,
            generated_time,
        )

        print(
            f"Saved {len(all_predictions)} "
            "prediction rows."
        )

        print(
            f"Generation ID: {generated_time}"
        )

    else:
        print(
            "Dry run: database was not modified."
        )


if __name__ == "__main__":
    main()
