"""Command-line entry point for UC-M-01 data preparation."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

# Support both `python -m ml.prepare_data` and `python ml/prepare_data.py`.
if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from ml.data_prep import DataPreparationError, default_database_path, prepare_training_data


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Prepare the UC-M-01 charging-load training dataset."
    )
    parser.add_argument(
        "--db",
        type=Path,
        default=default_database_path(),
        help="SQLite database path (defaults to the NCS application data location).",
    )
    parser.add_argument(
        "--project-root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="Project root where ml/data outputs are written.",
    )
    return parser


def main() -> int:
    args = build_parser().parse_args()
    try:
        metadata = prepare_training_data(args.db, args.project_root)
    except DataPreparationError as exc:
        print(f"[UC-M-01] ERROR: {exc}", file=sys.stderr)
        return 1
    except Exception as exc:  # pragma: no cover - defensive CLI boundary
        print(f"[UC-M-01] UNEXPECTED ERROR: {exc}", file=sys.stderr)
        return 2

    print("[UC-M-01] Training data preparation completed successfully.")
    print(json.dumps(metadata, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
