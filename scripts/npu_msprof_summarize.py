#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""汇总实机性能：host 逐 launch JSONL + msprof kernel_details.csv。

权威口径（2026-08-19，教材 KEM 多 launch）：
  * 设备计算真值 = kernel_details*.csv 各行 Task Duration 之和（按 kernel 名分组）
  * Host 同步墙钟 = output/npu_launch_metrics.jsonl 各 [npu_launch]（含 ACL 同步开销）
  * 进程墙钟 = [wall_sec]（含 host 准备 / D2H，不含 cmake/gen_data）
  * 禁止只用 msprof 终端那一行「task duration」代表整条 KEM

用法：
  python3 scripts/npu_msprof_summarize.py <case_dir>
  python3 scripts/npu_msprof_summarize.py --prof <prof_npu_or_OPPROF_dir> --jsonl <file>
"""

from __future__ import annotations

import argparse
import csv
import json
import os
import re
import sys
from collections import defaultdict
from typing import Dict, Iterable, List, Optional, Tuple


DURATION_KEYS = (
    "task duration(us)",
    "task duration(µs)",
    "duration(us)",
    "duration(µs)",
    "task duration",
    "duration",
    "time(us)",
)
NAME_KEYS = (
    "op name",
    "kernel name",
    "name",
    "op_name",
    "kernel_name",
    "opname",
)


def _norm(s: str) -> str:
    return re.sub(r"\s+", " ", s.strip().lower().replace("µ", "u"))


def _find_col(header: Iterable[str], keys: Iterable[str]) -> Optional[int]:
    normed = [_norm(h) for h in header]
    for want in keys:
        for i, h in enumerate(normed):
            if h == want or want in h:
                return i
    return None


def _parse_us(raw: str) -> Optional[float]:
    if raw is None:
        return None
    t = str(raw).strip().replace(",", "")
    if not t:
        return None
    m = re.search(r"[-+]?\d+(?:\.\d+)?", t)
    if not m:
        return None
    try:
        return float(m.group(0))
    except ValueError:
        return None


def parse_kernel_details_csv(path: str) -> List[Tuple[str, float]]:
    """返回 [(kernel_name, duration_us), ...]。跳过表头与空行。"""
    rows: List[Tuple[str, float]] = []
    with open(path, "r", encoding="utf-8", errors="replace", newline="") as fh:
        sample = fh.read(4096)
        fh.seek(0)
        try:
            dialect = csv.Sniffer().sniff(sample, delimiters=",;")
        except csv.Error:
            dialect = csv.excel
        reader = csv.reader(fh, dialect)
        header = None
        name_i = dur_i = None
        for rec in reader:
            if not rec or all(not str(c).strip() for c in rec):
                continue
            if header is None:
                header = rec
                name_i = _find_col(header, NAME_KEYS)
                dur_i = _find_col(header, DURATION_KEYS)
                if name_i is None or dur_i is None:
                    # 有的 CANN 把标题写在第二行；再读一行当 header
                    continue
                continue
            if name_i is None or dur_i is None or name_i >= len(rec) or dur_i >= len(rec):
                continue
            name = str(rec[name_i]).strip() or "unknown"
            us = _parse_us(rec[dur_i])
            if us is None:
                continue
            # 过滤明显非 kernel 的汇总行
            if _norm(name) in ("total", "sum", "n/a", "-"):
                continue
            rows.append((name, us))
    return rows


def collect_kernel_csvs(root: str) -> List[str]:
    found: List[str] = []
    if not os.path.isdir(root):
        return found
    for dirpath, _dirs, files in os.walk(root):
        for fn in files:
            low = fn.lower()
            if not low.endswith(".csv"):
                continue
            if "kernel_details" in low or low.startswith("op_summary") or "kernel details" in low:
                found.append(os.path.join(dirpath, fn))
    found.sort()
    return found


def parse_jsonl(path: str) -> List[dict]:
    out: List[dict] = []
    if not os.path.isfile(path):
        return out
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        for line in fh:
            line = line.strip()
            if not line:
                continue
            try:
                out.append(json.loads(line))
            except json.JSONDecodeError:
                continue
    return out


def parse_run_metrics_wall(path: str) -> Optional[str]:
    if not os.path.isfile(path):
        return None
    wall = None
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        for line in fh:
            m = re.search(r"^\[wall_sec\]\s+(\S+)", line)
            if m:
                wall = m.group(1)
    return wall


def summarize(case_dir: str, prof_root: Optional[str], jsonl_path: Optional[str], metrics_path: Optional[str]) -> int:
    case_dir = os.path.abspath(case_dir)
    if prof_root is None:
        cand = os.path.join(case_dir, "prof_npu")
        prof_root = cand if os.path.isdir(cand) else case_dir
    if jsonl_path is None:
        jsonl_path = os.path.join(case_dir, "output", "npu_launch_metrics.jsonl")
    if metrics_path is None:
        metrics_path = os.path.join(case_dir, "output", "run_metrics.txt")

    print("[msprof_sum] case={}".format(case_dir))
    print("[msprof_sum] 口径：设备真值=kernel_details 行求和；host=[npu_launch]；进程墙钟=[wall_sec]")

    csvs = collect_kernel_csvs(prof_root)
    per_name: Dict[str, List[float]] = defaultdict(list)
    total_us = 0.0
    n_rows = 0
    for csv_path in csvs:
        recs = parse_kernel_details_csv(csv_path)
        print("[msprof_sum] csv={} rows={}".format(csv_path, len(recs)))
        for name, us in recs:
            per_name[name].append(us)
            total_us += us
            n_rows += 1

    if n_rows == 0:
        print("[msprof_sum] WARN: 未解析到 kernel duration 行（尚未 RUN_WITH_MSPROF=1 或 csv 列名未识别）")
    else:
        print("[msprof_kernel_total] launches={} sum_us={:.1f} sum_ms={:.3f}".format(n_rows, total_us, total_us / 1000.0))
        for name in sorted(per_name, key=lambda k: -sum(per_name[k])):
            vals = per_name[name]
            s = sum(vals)
            print(
                "[msprof_kernel] name={} count={} sum_us={:.1f} mean_us={:.1f}".format(
                    name, len(vals), s, s / len(vals)
                )
            )

    launches = parse_jsonl(jsonl_path)
    if launches:
        hsum = 0.0
        for rec in launches:
            us = float(rec.get("duration_us", 0.0) or 0.0)
            hsum += us
            print(
                "[npu_launch_sum] seq={} name={} duration_us={:.1f} rc={}".format(
                    rec.get("seq", "?"), rec.get("name", "?"), us, rec.get("rc", "?")
                )
            )
        print("[npu_launch_total] count={} sum_us={:.1f} sum_ms={:.3f}".format(len(launches), hsum, hsum / 1000.0))
    else:
        print("[msprof_sum] WARN: 无 {}（host 未写入逐 launch 记录）".format(jsonl_path))

    wall = parse_run_metrics_wall(metrics_path)
    if wall:
        print("[msprof_sum] wall_sec={} (进程 kernel 段，含 host；非单 kernel)".format(wall))

    print("[msprof_sum] 教材填表：优先填 [msprof_kernel] 各 name 的 sum_us；整算子填 [msprof_kernel_total] sum_ms")
    return 0 if n_rows > 0 or launches else 0


def main(argv: Optional[List[str]] = None) -> int:
    p = argparse.ArgumentParser(description="Summarize NPU host launch metrics + msprof csv")
    p.add_argument("case_dir", nargs="?", default=".", help="用例根目录")
    p.add_argument("--prof", default=None, help="prof_npu 或 OPPROF 目录")
    p.add_argument("--jsonl", default=None, help="npu_launch_metrics.jsonl")
    p.add_argument("--metrics", default=None, help="run_metrics.txt")
    args = p.parse_args(argv)
    return summarize(args.case_dir, args.prof, args.jsonl, args.metrics)


if __name__ == "__main__":
    sys.exit(main())
