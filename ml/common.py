from __future__ import annotations

import os
import platform
from pathlib import Path


APP_DIR_NAME = "NCS_Charging_Platform"
DB_FILE_NAME = "charge_platform.db"


def default_database_path() -> Path:
    """
    Return the platform-specific database location used by the Qt app.

    Linux:
        ~/.local/share/NCS_Charging_Platform/charge_platform.db

    Windows:
        %LOCALAPPDATA%/NCS_Charging_Platform/charge_platform.db

    macOS:
        ~/Library/Application Support/NCS_Charging_Platform/charge_platform.db
    """

    system = platform.system().lower()

    if system == "windows":
        base = os.environ.get("LOCALAPPDATA")

        if base:
            return Path(base) / APP_DIR_NAME / DB_FILE_NAME

        return (
            Path.home()
            / "AppData"
            / "Local"
            / APP_DIR_NAME
            / DB_FILE_NAME
        )

    if system == "darwin":
        return (
            Path.home()
            / "Library"
            / "Application Support"
            / APP_DIR_NAME
            / DB_FILE_NAME
        )

    # Linux / Unix
    xdg_data_home = os.environ.get("XDG_DATA_HOME")

    if xdg_data_home:
        base = Path(xdg_data_home)
    else:
        base = Path.home() / ".local" / "share"

    return base / APP_DIR_NAME / DB_FILE_NAME


def resolve_database_path(custom_path: str | None = None) -> Path:
    if custom_path:
        return Path(custom_path).expanduser().resolve()

    return default_database_path()


def ensure_database_exists(path: Path) -> None:
    if not path.exists():
        raise FileNotFoundError(
            f"Database not found: {path}\n"
            "Run client_user or client_admin once so Qt initializes "
            "the database, or supply --db <database path>."
        )
