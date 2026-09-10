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
    "is_holiday",

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


def calculate_improvement(
    model_error: float,
    baseline_error: float,
) -> float:
    if baseline_error <= 1e-8:
        return 0.0

    return (
        (baseline_error - model_error)
        / baseline_error
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


    # ==========================================
    # Sample weights for sparse charging data
    # ==========================================

    positive_mask = (
        y_train > 0
    )

    positive_count = int(
        positive_mask.sum()
    )

    zero_count = int(
        (~positive_mask).sum()
    )


    if positive_count > 0:

        positive_weight = min(
            20.0,
            max(
                1.0,
                np.sqrt(
                    zero_count
                    / positive_count
                )
            ),
        )

    else:

        positive_weight = 1.0


    sample_weights = np.ones(
        len(y_train),
        dtype=float,
    )

    sample_weights[
        positive_mask.to_numpy()
    ] = positive_weight


    print()

    print(
        "Non-zero training rows:",
        positive_count,
        "/",
        len(y_train),
    )

    print(
        "Positive sample weight:",
        f"{positive_weight:.2f}"
    )


    pipeline.fit(
        x_train,
        y_train,
        model__sample_weight=sample_weights,
    )


    # ==========================================
    # Evaluate
    # ==========================================
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


    # ==========================================
    # Zero-threshold diagnostic
    # ==========================================

    print()
    print("=" * 72)
    print("Zero-threshold diagnostic")
    print("=" * 72)

    for threshold in [
        0.0,
        0.1,
        0.25,
        0.5,
        0.75,
        1.0,
        1.5,
        2.0,
        3.0,
        5.0,
    ]:

        threshold_predictions = (
            predictions.copy()
        )

        threshold_predictions[
            threshold_predictions
            < threshold
        ] = 0.0

        threshold_mae = mean_absolute_error(
            y_test,
            threshold_predictions,
        )

        threshold_rmse = np.sqrt(
            mean_squared_error(
                y_test,
                threshold_predictions,
            )
        )

        threshold_mape = calculate_mape(
            y_test.to_numpy(),
            threshold_predictions,
        )

        threshold_wmape = calculate_wmape(
            y_test.to_numpy(),
            threshold_predictions,
        )

        print(
            f"threshold={threshold:>4.2f} "
            f"MAE={threshold_mae:>8.4f} "
            f"RMSE={threshold_rmse:>8.4f} "
            f"MAPE={threshold_mape:>7.2f}% "
            f"WMAPE={threshold_wmape:>7.2f}%"
        )


    # ==========================================
    # RandomForest metrics
    # ==========================================

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


    # ==========================================
    # Naive baseline:
    # same hour one week ago
    # ==========================================

    baseline_predictions = (
        test_frame["load_lag_168h"]
        .to_numpy(dtype=float)
    )

    baseline_predictions = np.maximum(
        baseline_predictions,
        0.0,
    )

    baseline_mae = mean_absolute_error(
        y_test,
        baseline_predictions,
    )

    baseline_rmse = np.sqrt(
        mean_squared_error(
            y_test,
            baseline_predictions,
        )
    )

    baseline_mape = calculate_mape(
        y_test.to_numpy(),
        baseline_predictions,
    )

    baseline_wmape = calculate_wmape(
        y_test.to_numpy(),
        baseline_predictions,
    )


    # ==========================================
    # Improvement over baseline
    # ==========================================

    mae_improvement = calculate_improvement(
        mae,
        baseline_mae,
    )

    rmse_improvement = calculate_improvement(
        rmse,
        baseline_rmse,
    )

    mape_improvement = calculate_improvement(
        mape,
        baseline_mape,
    )

    wmape_improvement = calculate_improvement(
        wmape,
        baseline_wmape,
    )


    # ==========================================
    # Evaluation result
    # ==========================================

    print()
    print("=" * 72)
    print("Model evaluation")
    print("=" * 72)

    print(
        f"{'Metric':<12}"
        f"{'RandomForest':>18}"
        f"{'Naive baseline':>18}"
        f"{'Improvement':>18}"
    )

    print("-" * 72)

    print(
        f"{'MAE':<12}"
        f"{mae:>18.4f}"
        f"{baseline_mae:>18.4f}"
        f"{mae_improvement:>17.2f}%"
    )

    print(
        f"{'RMSE':<12}"
        f"{rmse:>18.4f}"
        f"{baseline_rmse:>18.4f}"
        f"{rmse_improvement:>17.2f}%"
    )

    print(
        f"{'MAPE':<12}"
        f"{mape:>17.2f}%"
        f"{baseline_mape:>17.2f}%"
        f"{mape_improvement:>17.2f}%"
    )

    print(
        f"{'WMAPE':<12}"
        f"{wmape:>17.2f}%"
        f"{baseline_wmape:>17.2f}%"
        f"{wmape_improvement:>17.2f}%"
    )


    # ==========================================
    # Baseline verdict
    # ==========================================

    beats_baseline = (
        mae < baseline_mae
        and rmse < baseline_rmse
        and mape < baseline_mape
    )

    print()
    print("-" * 72)

    if beats_baseline:
        print(
            "PASS: RandomForest outperforms "
            "the naive weekly baseline."
        )
    else:
        print(
            "WARNING: RandomForest does not "
            "outperform the naive baseline "
            "on all required metrics."
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
        "pipeline":
            pipeline,

        "numeric_features":
            NUMERIC_FEATURES,

        "categorical_features":
            CATEGORICAL_FEATURES,

        "target":
            TARGET_COLUMN,

        "random_seed":
            RANDOM_SEED,

        "metrics": {
            "mae":
                float(mae),

            "rmse":
                float(rmse),

            "mape":
                float(mape),

            "wmape":
                float(wmape),
        },

        "baseline": {
            "name":
                "same_hour_previous_week",

            "mae":
                float(baseline_mae),

            "rmse":
                float(baseline_rmse),

            "mape":
                float(baseline_mape),

            "wmape":
                float(baseline_wmape),
        },

        "baseline_improvement": {
            "mae_percent":
                float(mae_improvement),

            "rmse_percent":
                float(rmse_improvement),

            "mape_percent":
                float(mape_improvement),

            "wmape_percent":
                float(wmape_improvement),
        },

        "beats_baseline":
            bool(beats_baseline),

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
