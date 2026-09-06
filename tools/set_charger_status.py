#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""验收辅助工具（模拟管理端操作）：直接修改 charger 表电桩状态。

UC-U-02 验收标准：管理端把某个电桩状态改为使用中后，
用户端刷新，对应电站空闲数减 1。
本脚本模拟管理端的数据变更，供用户端验证空闲数联动。

用法：
  python3 tools/set_charger_status.py --db charge_platform.db --list
  python3 tools/set_charger_status.py --db charge_platform.db --charger 5 --status 1
  python3 tools/set_charger_status.py --db charge_platform.db --charger 5 --status 0

状态取值：0=空闲  1=使用中  2=故障
"""

import argparse
import sqlite3
import sys

STATUS_TEXT = {0: "空闲", 1: "使用中", 2: "故障"}


def list_chargers(db_path: str) -> None:
    con = sqlite3.connect(db_path)
    con.row_factory = sqlite3.Row
    cur = con.execute(
        """
        SELECT c.id, s.name AS station, c.charger_no,
               c.type, c.power, c.status
        FROM charger c JOIN station s ON s.id = c.station_id
        ORDER BY s.id, c.id
        """
    )
    rows = cur.fetchall()
    if not rows:
        print("charger 表为空")
        return
    print(f"{'id':>3}  {'电站':<12} {'编号':<8} {'类型':<4} {'功率':>5}  {'状态'}")
    for r in rows:
        ctype = "快充" if r["type"] == 1 else "慢充"
        print(f"{r['id']:>3}  {r['station']:<12} {r['charger_no']:<8} "
              f"{ctype:<4} {r['power']:>5.1f}  {STATUS_TEXT.get(r['status'], '?')}")
    con.close()


def set_status(db_path: str, charger_id: int, status: int) -> None:
    if status not in STATUS_TEXT:
        print(f"非法状态 {status}，允许值: {STATUS_TEXT}", file=sys.stderr)
        sys.exit(2)
    con = sqlite3.connect(db_path)
    con.execute("PRAGMA foreign_keys = ON")
    cur = con.execute(
        "UPDATE charger SET status = ? WHERE id = ?", (status, charger_id)
    )
    if cur.rowcount == 0:
        print(f"电桩 id={charger_id} 不存在", file=sys.stderr)
        con.close()
        sys.exit(1)
    con.commit()
    # 回读展示联动效果
    row = con.execute(
        """
        SELECT s.name, c.charger_no,
               (SELECT COUNT(*) FROM charger c2
                 WHERE c2.station_id = c.station_id AND c2.status = 0) AS free_cnt,
               (SELECT COUNT(*) FROM charger c2
                 WHERE c2.station_id = c.station_id) AS total_cnt
        FROM charger c JOIN station s ON s.id = c.station_id
        WHERE c.id = ?
        """,
        (charger_id,),
    ).fetchone()
    con.close()
    print(f"已将电桩 id={charger_id}（{row[0]} {row[1]}）置为「{STATUS_TEXT[status]}」")
    print(f"该电站当前空闲数/总桩数 = {row[2]}/{row[3]}（用户端刷新后应一致）")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--db", default="charge_platform.db", help="SQLite 数据库路径")
    parser.add_argument("--list", action="store_true", help="列出全部电桩")
    parser.add_argument("--charger", type=int, help="电桩 id")
    parser.add_argument("--status", type=int, choices=[0, 1, 2], help="目标状态")
    args = parser.parse_args()

    if args.list:
        list_chargers(args.db)
        return
    if args.charger is None or args.status is None:
        parser.print_help()
        sys.exit(2)
    set_status(args.db, args.charger, args.status)


if __name__ == "__main__":
    main()
