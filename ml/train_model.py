from __future__ import annotations

import argparse
from pathlib import Path

import joblib
import numpy as np
import pandas as pd

from sklearn.compose import ColumnTransformer
from sklearn.ensemble import RandomForestRegressor
from sklearn.metrics import (
    mean_absolute_error,
    mean_squared_error,
)
from sklearn.pipeline import Pipeline
from sklearn.preprocessing import OneHotEncoder


RANDOM_SEED = 42


NUMERIC_FEATURES = [
    "hour",
    "day_of_week",
    "day_of_month",
    "month",
    "is_weekend",

    "hour_sin",
    "hour_cos",
    "weekday_sin",
    "weekday_cos",

    "load_lag_1h",
    "load_lag_24h",
    "load_lag_168h",

    "load_avg_6h",
    "load_avg_24h",

    "orders_lag_1h",

    "temperature_c",
    "humidity",
    "is_rain",
]


CATEGORICAL_FEATURES = [
    "station_id",
]


TARGET_COLUMN = "load_kwh"


def load_dataset(path: Path) -> pd.DataFrame:
    if not path.exists():
        raise FileNotFoundError(
            f"Training data not found: {path}\n"
            "Run ml/prepare_data.py first."
        )

    frame = pd.read_csv(path)

    if frame.empty:
        raise ValueError(
            "Training dataset is empty."
        )

    required_columns = (
        NUMERIC_FEATURES
        + CATEGORICAL_FEATURES
        + [
            TARGET_COLUMN,
            "timestamp",
        ]
    )

    missing = [
        column
        for column in required_columns
        if column not in frame.columns
    ]

    if missing:
        raise ValueError(
            "Training dataset is missing columns: "
            + ", ".join(missing)
        )

    frame["timestamp"] = pd.to_datetime(
        frame["timestamp"],
        errors="coerce",
    )

    frame = frame.dropna(
        subset=["timestamp"]
    )

    frame = frame.sort_values(
        "timestamp"
    ).reset_index(drop=True)

    return frame


def chronological_split(
    frame: pd.DataFrame,
    test_ratio: float = 0.20,
):
    """
    Split by time instead of randomly.

    Earlier rows -> training
    Later rows   -> testing

    This is more appropriate for forecasting because
    future data should not leak into the training set.
    """

    split_index = int(
        len(frame) * (1.0 - test_ratio)
    )

    if split_index <= 0:
        raise ValueError(
            "Not enough rows for training."
        )

    if split_index >= len(frame):
        raise ValueError(
            "Not enough rows for testing."
        )

    train = frame.iloc[
        :split_index
    ].copy()

    test = frame.iloc[
        split_index:
    ].copy()

    return train, test


def calculate_mape(
    actual: np.ndarray,
    predicted: np.ndarray,
) -> float:
    """
    Standard MAPE cannot divide by zero.

    Charging-load data contains many zero-demand hours,
    so calculate MAPE only where the actual load > 0.
    """

    actual = np.asarray(
        actual,
        dtype=float,
    )

    predicted = np.asarray(
        predicted,
        dtype=float,
    )

    mask = np.abs(actual) > 1e-8

    if not np.any(mask):
        return 0.0

    percentage_errors = (
        np.abs(
            (
                actual[mask]
                - predicted[mask]
            )
            / actual[mask]
        )
    )

    return float(
        np.mean(percentage_errors)
        * 100.0
    )


def calculate_wmape(
    actual: np.ndarray,
    predicted: np.ndarray,
) -> float:
    """
    Weighted MAPE remains useful when there are many
    zero-load hours.
    """

    actual = np.asarray(
        actual,
        dtype=float,
    )

    predicted = np.asarray(
        predicted,
        dtype=float,
    )

    denominator = np.sum(
        np.abs(actual)
    )

    if denominator <= 1e-8:
        return 0.0

    return float(
        np.sum(
            np.abs(actual - predicted)
        )
        / denominator
        * 100.0
    )


def build_pipeline() -> Pipeline:
    preprocessing = ColumnTransformer(
        transformers=[
            (
                "numeric",
                "passthrough",
                NUMERIC_FEATURES,
            ),
            (
                "station",
                OneHotEncoder(
                    handle_unknown="ignore",
                    sparse_output=False,
                ),
                CATEGORICAL_FEATURES,
            ),
        ]
    )

    model = RandomForestRegressor(
        n_estimators=300,
        max_depth=16,
        min_samples_split=4,
        min_samples_leaf=2,

        random_state=RANDOM_SEED,

        n_jobs=-1,
    )

    return Pipeline(
        steps=[
            (
                "preprocessing",
                preprocessing,
            ),
            (
                "model",
                model,
            ),
        ]
    )


def main():
    parser = argparse.ArgumentParser(
        description=(
            "Train the charging-load "
            "RandomForest model."
        )
    )

    parser.add_argument(
        "--input",
        default="ml/training_data.csv",
        help="Training CSV path",
    )

    parser.add_argument(
        "--model",
        default="ml/models/load_rf.pkl",
        help="Output model path",
    )

    parser.add_argument(
        "--test-ratio",
        type=float,
        default=0.20,
        help="Fraction of latest rows used for testing",
    )

    args = parser.parse_args()

    np.random.seed(
        RANDOM_SEED
    )

    input_path = Path(
        args.input
    )

    model_path = Path(
        args.model
    )


    # ==========================================
    # Load data
    # ==========================================

    frame = load_dataset(
        input_path
    )

    print(
        f"Dataset rows: {len(frame)}"
    )

    print(
        "Time range:",
        frame["timestamp"].min(),
        "->",
        frame["timestamp"].max(),
    )


    # ==========================================
    # Train / test split
    # ==========================================

    train_frame, test_frame = (
        chronological_split(
            frame,
            args.test_ratio,
        )
    )

    print()
    print(
        f"Training rows: {len(train_frame)}"
    )

    print(
        f"Testing rows:  {len(test_frame)}"
    )

    print(
        "Training range:",
        train_frame["timestamp"].min(),
        "->",
        train_frame["timestamp"].max(),
    )

    print(
        "Testing range:",
        test_frame["timestamp"].min(),
        "->",
        test_frame["timestamp"].max(),
    )


    feature_columns = (
        NUMERIC_FEATURES
        + CATEGORICAL_FEATURES
    )

    x_train = train_frame[
        feature_columns
    ]

    y_train = train_frame[
        TARGET_COLUMN
    ]

    x_test = test_frame[
        feature_columns
    ]

    y_test = test_frame[
        TARGET_COLUMN
    ]


    # ==========================================
    # Train RandomForest
    # ==========================================

    pipeline = build_pipeline()

    print()
    print(
        "Training RandomForestRegressor..."
    )

    pipeline.fit(
        x_train,
        y_train,
    )


    # ==========================================
    # Evaluate
    # ==========================================

    predictions = pipeline.predict(
        x_test
    )

    # Loads must never be negative.
    predictions = np.maximum(
        predictions,
        0.0,
    )


    mae = mean_absolute_error(
        y_test,
        predictions,
    )

    rmse = np.sqrt(
        mean_squared_error(
            y_test,
            predictions,
        )
    )

    mape = calculate_mape(
        y_test.to_numpy(),
        predictions,
    )

    wmape = calculate_wmape(
        y_test.to_numpy(),
        predictions,
    )


    print()
    print("=" * 45)
    print("Model evaluation")
    print("=" * 45)

    print(
        f"MAE:   {mae:.4f} kWh"
    )

    print(
        f"RMSE:  {rmse:.4f} kWh"
    )

    print(
        f"MAPE:  {mape:.2f}% "
        "(non-zero actual hours)"
    )

    print(
        f"WMAPE: {wmape:.2f}%"
    )


    # ==========================================
    # Additional sanity information
    # ==========================================

    non_zero_test = (
        y_test > 0
    ).sum()

    print()
    print(
        f"Non-zero test hours: "
        f"{non_zero_test}/{len(y_test)}"
    )

    print(
        "Actual average load:",
        f"{y_test.mean():.4f} kWh"
    )

    print(
        "Predicted average load:",
        f"{predictions.mean():.4f} kWh"
    )


    # ==========================================
    # Save model
    # ==========================================

    model_path.parent.mkdir(
        parents=True,
        exist_ok=True,
    )


    model_package = {
        "pipeline": pipeline,

        "numeric_features":
            NUMERIC_FEATURES,

        "categorical_features":
            CATEGORICAL_FEATURES,

        "target":
            TARGET_COLUMN,

        "random_seed":
            RANDOM_SEED,

        "metrics": {
            "mae": float(mae),
            "rmse": float(rmse),
            "mape": float(mape),
            "wmape": float(wmape),
        },

        "training_rows":
            len(train_frame),

        "testing_rows":
            len(test_frame),

        "training_end":
            str(
                train_frame[
                    "timestamp"
                ].max()
            ),
    }


    joblib.dump(
        model_package,
        model_path,
    )


    print()
    print(
        f"Model saved to: {model_path}"
    )

    print()
    print(
        "Training completed successfully."
    )


if __name__ == "__main__":
    main()
