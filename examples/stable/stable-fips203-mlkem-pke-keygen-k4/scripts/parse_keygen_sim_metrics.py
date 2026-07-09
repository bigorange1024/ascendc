#!/usr/bin/env python3
# @probe stable-fips203-mlkem-pke-keygen-k4
# @file scripts/parse_keygen_sim_metrics.py
# @layer script
# @role 探针脚本：支持 golden、LUT 生成、验证或 SIM 编排。 / Helper script `parse_keygen_sim_metrics.py`.
# @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
# @launch N/A（host / 脚本 / CMake 不参与 device launch）
# @ai_core N/A（非 AI Core 内核源）
# @depends Python3 标准库；可能 import 同目录 keygen_golden / numpy。
# @verify 随 run.sh 全链或子目录 run_orchestrated/sim_*.sh 验收。

# @exp stable-fips203-mlkem-pke-keygen-k4
# coding=utf-8
"""
本文件在 KeyGen 流水线中的位置：Host：KeyGen 输入、golden、KAT、验收脚本。
对齐：FIPS 203 Alg.13 / ML-KEM-1024（k=4）。
与 golden 关系：仅 I/O 等价验收；禁止把 Host/参考源码当作 AscendC 实现规格。
文件：scripts/parse_keygen_sim_metrics.py
"""
"""解析 KeyGen SIM 两次 launch 的 tick 与墙钟用时，并打印加总摘要。

数据来源（优先级）：
  1. sim_log/profile_task_log0.toml — task0=prep、task1=mmad 的 duration（tick）
  2. kernel 日志 — LaunchKernel→TaskCompleted 墙钟 ms（用时）
  3. kernel 日志 — 末行 Total tick；若无 profile_task，按用时比例分摊 tick
"""
from __future__ import annotations

import re
import sys
from datetime import datetime
from pathlib import Path

LABELS = (
    "prep f203_keygen_prep (行3-15)",
    "compute mmad_custom (行16-21)",
)
KERNEL_MARKERS = (
    "kernel_name=f203_keygen_prep",
    "kernel_name=mmad_custom",
)
TASK_DONE_MARKERS = (
    "task_id=0 has been completed",
    "task_id=1 has been completed",
)


def _parse_ts_ms(line: str) -> float | None:
    m = re.search(r"(\d{4}-\d{2}-\d{2}-\d{2}:\d{2}:\d{2}\.\d+)", line)
    if not m:
        return None
    raw = m.group(1)
    head, frac = raw.rsplit(".", 1)
    dt = datetime.strptime(head, "%Y-%m-%d-%H:%M:%S")
    return dt.timestamp() * 1000.0 + float(f"0.{frac}")


# 本函数为 KeyGen 流水线组件 `_find_profile_task`（详见 STATUS/customspec）。
def _find_profile_task(case_dir: Path) -> Path | None:
    for base in (case_dir / "sim_log", case_dir):
        p = base / "profile_task_log0.toml"
        if p.is_file():
            return p
    return None


def _parse_profile_ticks(profile_path: Path) -> list[int]:
    text = profile_path.read_text(encoding="utf-8", errors="replace")
    ticks: list[int] = []
    for task_id in (0, 1):
        m = re.search(
            rf"\[network\.stream3\.task{task_id}\][\s\S]*?\nduration = (\d+)",
            text,
        )
        if m:
            ticks.append(int(m.group(1)))
    return ticks


# 本函数为 KeyGen 流水线组件 `_parse_wall_ms_per_launch`（详见 STATUS/customspec）。
def _parse_wall_ms_per_launch(log_text: str) -> list[float]:
    lines = log_text.splitlines()
    starts: list[float | None] = [None, None]
    ends: list[float | None] = [None, None]

    for line in lines:
        ts = _parse_ts_ms(line)
        if ts is None:
            continue
        for i, marker in enumerate(KERNEL_MARKERS):
            if marker in line and "LaunchKernelWithHandle" in line:
                starts[i] = ts
        for i, marker in enumerate(TASK_DONE_MARKERS):
            if marker in line and "SynchronizeImpl" in line:
                ends[i] = ts

    out: list[float] = []
    for i in range(2):
        if starts[i] is not None and ends[i] is not None and ends[i] >= starts[i]:
            out.append(ends[i] - starts[i])
        else:
            out.append(0.0)
    return out


# 本函数为 KeyGen 流水线组件 `_parse_total_tick`（详见 STATUS/customspec）。
def _parse_total_tick(log_text: str) -> int | None:
    hits = [int(x) for x in re.findall(r"Total tick:\s*(\d+)", log_text)]
    return hits[-1] if hits else None


def _parse_total_model_ms(log_text: str) -> float:
    hits = [float(x) for x in re.findall(r"Model RUN TIME:\s*([\d.]+)\s*ms", log_text)]
    return hits[-1] if hits else 0.0


def build_summary(case_dir: Path, kernel_log: Path | None = None) -> dict:
    case_dir = case_dir.resolve()
    log_path = kernel_log or (case_dir / "output" / "keygen_kernel.log")
    log_text = log_path.read_text(encoding="utf-8", errors="replace") if log_path.is_file() else ""

    profile = _find_profile_task(case_dir)
    ticks = _parse_profile_ticks(profile) if profile else []
    wall_ms = _parse_wall_ms_per_launch(log_text)
    total_tick = _parse_total_tick(log_text)
    total_model_ms = _parse_total_model_ms(log_text)

    if len(ticks) < 2 and total_tick is not None:
        wsum = sum(wall_ms)
        if wsum > 0 and len(wall_ms) == 2:
            t0 = int(round(total_tick * wall_ms[0] / wsum))
            ticks = [t0, max(0, total_tick - t0)]
        elif total_tick is not None and not ticks:
            ticks = [total_tick]

    while len(wall_ms) < 2:
        wall_ms.append(0.0)
    while len(ticks) < 2:
        ticks.append(0)

    return {
        "labels": LABELS,
        "ticks": ticks[:2],
        "wall_ms": wall_ms[:2],
        "total_tick": sum(ticks[:2]) if ticks else (total_tick or 0),
        "total_wall_ms": sum(wall_ms[:2]),
        "total_model_ms": total_model_ms,
        "profile_path": str(profile) if profile else None,
        "kernel_log": str(log_path),
    }


# 本函数为 KeyGen 流水线组件 `format_summary_lines`（详见 STATUS/customspec）。
def format_summary_lines(summary: dict) -> list[str]:
    lines: list[str] = []
    for i in range(2):
        tick = summary["ticks"][i]
        ms = summary["wall_ms"][i]
        ms_part = f" wall_ms={ms:.1f}" if ms > 0 else ""
        lines.append(f"[keygen] launch {i + 1}: {summary['labels'][i]} tick={tick}{ms_part}")
    lines.append(
        "[keygen] SIM summary: 2 launch(es) "
        f"total_tick={summary['total_tick']} "
        f"total_wall_ms={summary['total_wall_ms']:.1f} "
        f"total_model_ms={summary['total_model_ms']:.1f}"
    )
    if summary.get("profile_path"):
        lines.append(f"[keygen] SIM profile: {summary['profile_path']}")
    return lines


# 本函数为 KeyGen 流水线组件 `print_summary`（详见 STATUS/customspec）。
def print_summary(case_dir: Path, kernel_log: Path | None = None) -> dict:
    summary = build_summary(case_dir, kernel_log)
    for line in format_summary_lines(summary):
        print(line, flush=True)
    return summary


def main() -> int:
    case_dir = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path.cwd()
    kernel_log = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else None
    print_summary(case_dir, kernel_log)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
