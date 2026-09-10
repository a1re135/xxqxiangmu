from __future__ import annotations

import argparse
from pathlib import Path

import joblib
import numpy as np
import pandas as pd


def main():
    parser = argparse.ArgumentParser(
        description="Evaluate charging-load model."
    )

    parser.add_argument(
        "--input",
        default="ml/training_data.csv",
    )

    parser.add_argument(
        "--model",
        default="ml/models/load_rf.pkl",
    )

    parser.add_argument(
        "--station",
        type=int,
        default=1,
    )

    parser.add_argument(
        "--hours",
        type=int,
        choices=[6, 24],
        default=24,
    )

    parser.add_argument(
        "--output",
        default="ml/evaluation_results.csv",
    )

    args = parser.parse_args()

    model_path = Path(args.model)
    input_path = Path(args.input)

    if not model_path.exists():
        raise FileNotFoundError(
            f"Model not found: {model_path}"
        )

    if not input_path.exists():
        raise FileNotFoundError(
            f"Training data not found: {input_path}"
        )

    package = joblib.load(model_path)

    pipeline = package["pipeline"]

    numeric_features = package[
        "numeric_features"
    ]

    categorical_features = package[
        "categorical_features"
    ]

    features = (
        numeric_features
        + categorical_features
    )

    df = pd.read_csv(input_path)

    df["timestamp"] = pd.to_datetime(
        df["timestamp"]
    )

    station_df = df[
        df["station_id"] == args.station
    ].copy()

    station_df = station_df.sort_values(
        "timestamp"
    )

    if station_df.empty:
        raise ValueError(
            f"No data for station {args.station}."
        )

    # Use the most recent historical hours.
    evaluation_df = station_df.tail(
        args.hours
    ).copy()

    x = evaluation_df[features]

    actual = evaluation_df[
        "load_kwh"
    ].to_numpy()

    predicted = pipeline.predict(x)

    predicted = np.maximum(
        predicted,
        0.0,
    )

    result = pd.DataFrame({
        "timestamp":
            evaluation_df["timestamp"],

        "station_id":
            evaluation_df["station_id"],

        "actual_load":
            actual,

        "predicted_load":
            predicted,
    })

    result["absolute_error"] = (
        np.abs(
            result["actual_load"]
            - result["predicted_load"]
        )
    )

    output_path = Path(args.output)

    output_path.parent.mkdir(
        parents=True,
        exist_ok=True,
    )

    result.to_csv(
        output_path,
        index=False,
    )

    print(
        result.to_string(
            index=False
        )
    )

    print()
    print(
        f"Rows: {len(result)}"
    )

    print(
        "Mean absolute error:",
        f"{result['absolute_error'].mean():.4f} kWh"
    )

    print(
        f"Saved to: {output_path}"
    )


if __name__ == "__main__":
    main()
