from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


ML_DIR = Path(__file__).resolve().parent


def run_script(script_name: str, arguments: list[str] | None = None) -> None:
    script_path = ML_DIR / script_name

    if not script_path.exists():
        raise FileNotFoundError(
            f"Script not found: {script_path}"
        )

    command = [
        sys.executable,
        str(script_path),
    ]

    if arguments:
        command.extend(arguments)

    print()
    print("=" * 70)
    print("Running:")
    print(" ".join(command))
    print("=" * 70)

    result = subprocess.run(
        command,
        cwd=ML_DIR.parent,
    )

    if result.returncode != 0:
        raise RuntimeError(
            f"{script_name} failed "
            f"with exit code {result.returncode}"
        )


def command_train(args) -> None:
    print("Preparing training data...")

    prepare_args = []

    if args.db:
        prepare_args.extend([
            "--db",
            args.db,
        ])

    run_script(
        "prepare_data.py",
        prepare_args,
    )

    print()
    print("Training RandomForest model...")

    run_script(
        "train_model.py"
    )

    print()
    print("Training pipeline completed successfully.")


def command_predict(args) -> None:
    prediction_args = [
        "--horizon",
        str(args.horizon),
    ]

    if args.station is not None:
        prediction_args.extend([
            "--station",
            str(args.station),
        ])

    if args.db:
        prediction_args.extend([
            "--db",
            args.db,
        ])

    if args.no_write:
        prediction_args.append(
            "--no-write"
        )

    run_script(
        "predict.py",
        prediction_args,
    )

def command_refresh(args) -> None:
    """
    Rebuild the dataset, retrain the model,
    then generate a fresh prediction.
    """

    print()
    print("=" * 70)
    print("STEP 1/3 - Preparing training data")
    print("=" * 70)

    prepare_args = []

    if args.db:
        prepare_args.extend([
            "--db",
            args.db,
        ])

    run_script(
        "prepare_data.py",
        prepare_args,
    )


    print()
    print("=" * 70)
    print("STEP 2/3 - Training RandomForest model")
    print("=" * 70)

    run_script(
        "train_model.py"
    )


    print()
    print("=" * 70)
    print("STEP 3/3 - Generating prediction")
    print("=" * 70)

    prediction_args = [
        "--horizon",
        str(args.horizon),
    ]

    if args.station is not None:
        prediction_args.extend([
            "--station",
            str(args.station),
        ])

    if args.db:
        prediction_args.extend([
            "--db",
            args.db,
        ])

    run_script(
        "predict.py",
        prediction_args,
    )


    print()
    print("=" * 70)
    print("ML refresh completed successfully.")
    print("=" * 70)

def command_evaluate(args) -> None:
    evaluation_args = [
        "--station",
        str(args.station),

        "--hours",
        str(args.hours),
    ]

    if args.output:
        evaluation_args.extend([
            "--output",
            args.output,
        ])

    run_script(
        "evaluate_model.py",
        evaluation_args,
    )


def command_check(args) -> None:
    check_args = []

    if args.db:
        check_args.extend([
            "--db",
            args.db,
        ])

    run_script(
        "check_db.py",
        check_args,
    )


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "NCS Charging Platform "
            "machine-learning command line tool"
        )
    )

    subparsers = parser.add_subparsers(
        dest="command",
        required=True,
    )


    # ============================================================
    # train
    # ============================================================

    train_parser = subparsers.add_parser(
        "train",
        help="Prepare data and train the RandomForest model.",
    )

    train_parser.add_argument(
        "--db",
        default=None,
        help="Optional SQLite database path.",
    )

    train_parser.set_defaults(
        function=command_train
    )


    # ============================================================
    # predict
    # ============================================================

    predict_parser = subparsers.add_parser(
        "predict",
        help="Generate future load predictions.",
    )

    predict_parser.add_argument(
        "--station",
        type=int,
        default=None,
        help=(
            "Station ID. "
            "If omitted, predict all stations."
        ),
    )

    predict_parser.add_argument(
        "--horizon",
        type=int,
        choices=[1, 6, 24],
        default=6,
        help="Prediction horizon in hours.",
    )

    predict_parser.add_argument(
        "--db",
        default=None,
        help="Optional SQLite database path.",
    )

    predict_parser.add_argument(
        "--no-write",
        action="store_true",
        help="Do not write predictions to SQLite.",
    )

    predict_parser.set_defaults(
        function=command_predict
    )

    # ============================================================
    # refresh
    #
    # prepare data -> train -> predict
    # ============================================================

    refresh_parser = subparsers.add_parser(
        "refresh",
        help=(
            "Prepare data, retrain the model, "
            "and generate fresh predictions."
        ),
    )

    refresh_parser.add_argument(
        "--station",
        type=int,
        default=None,
        help=(
            "Station ID. "
            "If omitted, predict all stations."
        ),
    )

    refresh_parser.add_argument(
        "--horizon",
        type=int,
        choices=[1, 6, 24],
        default=6,
        help="Prediction horizon in hours.",
    )

    refresh_parser.add_argument(
        "--db",
        default=None,
        help="Optional SQLite database path.",
    )

    refresh_parser.set_defaults(
        function=command_refresh
    )
    
    # ============================================================
    # evaluate
    # ============================================================

    evaluate_parser = subparsers.add_parser(
        "evaluate",
        help="Compare historical actual and predicted load.",
    )

    evaluate_parser.add_argument(
        "--station",
        type=int,
        required=True,
        help="Station ID.",
    )

    evaluate_parser.add_argument(
        "--hours",
        type=int,
        choices=[6, 24],
        default=24,
        help="Number of historical hours to evaluate.",
    )

    evaluate_parser.add_argument(
        "--output",
        default="ml/evaluation_results.csv",
        help="Evaluation CSV output path.",
    )

    evaluate_parser.set_defaults(
        function=command_evaluate
    )


    # ============================================================
    # check
    # ============================================================

    check_parser = subparsers.add_parser(
        "check",
        help="Check SQLite database availability.",
    )

    check_parser.add_argument(
        "--db",
        default=None,
        help="Optional SQLite database path.",
    )

    check_parser.set_defaults(
        function=command_check
    )

    return parser


def main() -> None:
    parser = build_parser()

    args = parser.parse_args()

    try:
        args.function(args)

    except Exception as error:
        print()
        print(f"ERROR: {error}")
        sys.exit(1)


if __name__ == "__main__":
    main()
