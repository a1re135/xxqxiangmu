#!/usr/bin/env python3
"""
UC-W-02: SQLite -> web/public/data/dashboard.json exporter.

The browser never connects to SQLite directly. Only this exporter reads the
database and writes JSON for the Web dashboard.

Examples:
  python3 tools/export_dashboard.py --db /path/to/ncs.db --once
  python3 tools/export_dashboard.py --db /path/to/ncs.db --interval 30
"""
from __future__ import annotations

import argparse
import json
import sqlite3
import time
from datetime import date, datetime, timedelta
from pathlib import Path

WEB_DIR = Path(__file__).resolve().parents[1]
OUTPUT = WEB_DIR / "public" / "data" / "dashboard.json"

def find_db(explicit: str | None) -> Path:
    if explicit:
        p = Path(explicit).expanduser().resolve()
        if not p.exists():
            raise FileNotFoundError(f"数据库文件不存在: {p}")
        return p

    candidates = [
        Path.home() / ".local" / "share" / "NCS_Charging_Platform" / "charge_platform.db",
        WEB_DIR.parent / "data" / "ncs.db",
        WEB_DIR.parent / "database" / "ncs.db",
        WEB_DIR.parent / "ncs.db",
        WEB_DIR / "ncs.db",
    ]
    for p in candidates:
        if p.exists():
            return p.resolve()

    raise FileNotFoundError(
        "未找到 SQLite 数据库，请使用 --db 指定数据库路径。"
    )

def scalar(cur: sqlite3.Cursor, sql: str, params=(), default=0):
    row = cur.execute(sql, params).fetchone()
    return default if row is None or row[0] is None else row[0]

def build_dashboard(db_path: Path) -> dict:
    uri = f"file:{db_path.as_posix()}?mode=ro"
    con = sqlite3.connect(uri, uri=True)
    try:
        cur = con.cursor()

        now = datetime.now()
        today = now.strftime("%Y-%m-%d")
        month = now.strftime("%Y-%m")

        total_orders = int(scalar(
            cur, "SELECT COUNT(*) FROM charging_order WHERE status=2"
        ))
        total_revenue = float(scalar(
            cur, "SELECT COALESCE(SUM(amount),0) "
                 "FROM charging_order WHERE status=2"
        ))
        registered_users = int(scalar(cur, "SELECT COUNT(*) FROM user"))
        online_chargers = int(scalar(
            cur, "SELECT COUNT(*) FROM charger WHERE status IN (0,1)"
        ))

        # 30 days, including today, filled with zero for missing dates.
        start = now.date() - timedelta(days=29)
        revenue_rows = cur.execute("""
            SELECT substr(start_time,1,10) AS d, COALESCE(SUM(amount),0)
            FROM charging_order
            WHERE status=2
              AND substr(start_time,1,10) BETWEEN ? AND ?
            GROUP BY substr(start_time,1,10)
            ORDER BY d
        """, (start.isoformat(), today)).fetchall()
        revenue_map = {row[0]: float(row[1]) for row in revenue_rows}
        revenue30 = []
        for i in range(30):
            d = start + timedelta(days=i)
            revenue30.append({
                "date": d.strftime("%m-%d"),
                "value": round(revenue_map.get(d.isoformat(), 0.0), 2),
            })

        # Station ranking by completed-order charging energy (kWh).
        station_rows = cur.execute("""
            SELECT s.name, COALESCE(SUM(o.energy),0) AS total
            FROM charging_order o
            JOIN charger c ON c.id=o.charger_id
            JOIN station s ON s.id=c.station_id
            WHERE o.status=2
            GROUP BY s.id, s.name
            ORDER BY total DESC
            LIMIT 6
        """).fetchall()
        station_ranking = [
            {"name": row[0], "value": round(float(row[1]),2)}
            for row in station_rows
        ]

        status_rows = cur.execute(
            "SELECT status, COUNT(*) FROM charger GROUP BY status"
        ).fetchall()
        status_map = {int(row[0]): int(row[1]) for row in status_rows}

        type_rows = cur.execute(
            "SELECT type, COUNT(*) FROM charger GROUP BY type ORDER BY type"
        ).fetchall()
        charge_type = []
        for t, count in type_rows:
            charge_type.append({
                "name": "快充" if int(t) == 1 else "慢充",
                "value": int(count),
            })

        hourly_usage = []
        for h in range(24):
            hh = f"{h:02d}"
            count = scalar(
                cur,
                "SELECT COUNT(*) FROM charging_order "
                "WHERE status=2 AND substr(start_time,12,2)=?",
                (hh,),
                0,
            )
            hourly_usage.append(int(count))

        # Export a few additional values while keeping existing UI compatible.
        today_revenue = float(scalar(
            cur,
            "SELECT COALESCE(SUM(amount),0) "
            "FROM charging_order "
            "WHERE status=2 AND substr(start_time,1,10)=?",
            (today,), 0
        ))
        month_revenue = float(scalar(
            cur,
            "SELECT COALESCE(SUM(amount),0) "
            "FROM charging_order "
            "WHERE status=2 AND substr(start_time,1,7)=?",
            (month,), 0
        ))

        return {
            "updatedAt": now.strftime("%Y-%m-%d %H:%M:%S"),
            "source": {
                "type": "sqlite",
                "database": str(db_path),
                "generatedBy": "tools/export_dashboard.py",
            },
            "kpis": {
                "totalOrders": total_orders,
                "totalRevenue": round(total_revenue,2),
                "onlineChargers": online_chargers,
                "registeredUsers": registered_users,
                "todayRevenue": round(today_revenue,2),
                "monthRevenue": round(month_revenue,2),
            },
            "chargerStatus": [
                {"name":"闲置", "value":status_map.get(0,0)},
                {"name":"在用", "value":status_map.get(1,0)},
                {"name":"故障", "value":status_map.get(2,0)},
            ],
            "stationRanking": station_ranking,
            "revenue30d": revenue30,
            "hourlyUsage": hourly_usage,
            "chargeType": charge_type,
            "forecast24h": [],
        }
    finally:
        con.close()

def export_once(db_path: Path) -> None:
    data = build_dashboard(db_path)
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    temp = OUTPUT.with_suffix(".json.tmp")
    temp.write_text(
        json.dumps(data, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )
    temp.replace(OUTPUT)
    print(f"[UC-W-02] {data['updatedAt']} -> {OUTPUT}")

def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--db", help="SQLite database path")
    parser.add_argument(
        "--once", action="store_true",
        help="export once and exit"
    )
    parser.add_argument(
        "--interval", type=int, default=30,
        help="export interval in seconds (default: 30)"
    )
    args = parser.parse_args()

    db_path = find_db(args.db)

    if args.once:
        export_once(db_path)
        return

    while True:
        try:
            export_once(db_path)
        except Exception as exc:
            print(f"[UC-W-02] export failed: {exc}")
        time.sleep(max(1, args.interval))

if __name__ == "__main__":
    main()
