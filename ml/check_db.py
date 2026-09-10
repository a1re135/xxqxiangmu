import argparse
import sqlite3

from common import (
    ensure_database_exists,
    resolve_database_path,
)


def main():
    parser = argparse.ArgumentParser()

    parser.add_argument(
        "--db",
        default=None,
        help="Optional path to charge_platform.db",
    )

    args = parser.parse_args()

    db_path = resolve_database_path(args.db)
    ensure_database_exists(db_path)

    print(f"Database: {db_path}")

    connection = sqlite3.connect(db_path)

    try:
        cursor = connection.cursor()

        cursor.execute("SELECT COUNT(*) FROM station")
        station_count = cursor.fetchone()[0]

        cursor.execute("SELECT COUNT(*) FROM charger")
        charger_count = cursor.fetchone()[0]

        cursor.execute(
            """
            SELECT COUNT(*)
            FROM charging_order
            WHERE status = 2
            """
        )
        completed_order_count = cursor.fetchone()[0]

        print(f"Stations:         {station_count}")
        print(f"Chargers:        {charger_count}")
        print(f"Completed orders:{completed_order_count}")

    finally:
        connection.close()


if __name__ == "__main__":
    main()
