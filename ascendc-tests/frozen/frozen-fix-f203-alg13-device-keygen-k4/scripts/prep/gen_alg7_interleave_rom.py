#!/usr/bin/env python3
# @probe pass-fix-f203-alg13-device-keygen-k4
# @file scripts/prep/gen_alg7_interleave_rom.py
# @layer script
# @role 探针脚本：支持 golden、LUT 生成、验证或 SIM 编排。 / Helper script `gen_alg7_interleave_rom.py`.
# @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
# @launch N/A（host / 脚本 / CMake 不参与 device launch）
# @ai_core N/A（非 AI Core 内核源）
# @depends Python3 标准库；可能 import 同目录 keygen_golden / numpy。
# @verify 随 run.sh 全链或子目录 run_orchestrated/sim_*.sh 验收。

# coding=utf-8
"""生成 Alg.7 rej 阶段 d1/d2 交错 Gather 字节索引 ROM → f203_alg7_interleave_rom.h。

设备侧 rej 向量路径将 scratch 布局视为连续字节流：
  scratch = d1[CAND_PAIRS] int32 || d2[CAND_PAIRS] int32
规范 rej 扫描顺序为 stream[2*k]=d1[k], stream[2*k+1]=d2[k]（共 STREAM_LEN=448 lane）。

本脚本为每个 stream lane 预计算 Gather 的**源字节偏移**（非元素下标）：
  - d1[k] 位于字节 4*k
  - d2[k] 位于字节 D2_BYTE_BASE + 4*k，其中 D2_BYTE_BASE = NPAIRS×4

输出头文件供 f203_alg7_rej_vec.hpp / compact 路径在 UB 上做 AscendC::Gather 重排。
须与 alg7_geom.CAND_PAIRS、STREAM_LEN 及 f203_alg7_layout.h 同步。
"""
from __future__ import annotations

from pathlib import Path

from alg7_geom import CAND_PAIRS, STREAM_LEN

NPAIRS = CAND_PAIRS
OUT_LEN = STREAM_LEN
# d2 数组在 scratch 字节流中的起始偏移（紧接 d1 的 224×4 字节之后）
D2_BYTE_BASE = NPAIRS * 4  # d2[k] 在 scratch=t1||t2 中的字节偏移起点


def main() -> None:
    # 按规范顺序生成 448 个 Gather 字节偏移：偶数 lane→d1，奇数 lane→d2
    idx: list[int] = []
    for k in range(NPAIRS):
        idx.append(4 * k)  # stream[2*k]   ← d1[k] 的首字节
        idx.append(D2_BYTE_BASE + 4 * k)  # stream[2*k+1] ← d2[k] 的首字节

    if len(idx) != OUT_LEN:
        raise SystemExit(f"idx len {len(idx)} != {OUT_LEN}")

    out_h = Path(__file__).resolve().parents[2] / "prep" / "alg7" / "f203_alg7_interleave_rom.h"
    lines = [
        "/**",
        " * @file f203_alg7_interleave_rom.h",
        f" * @brief Alg.7 rej：d1[{NPAIRS}]||d2[{NPAIRS}] scratch → stream[{OUT_LEN}] Gather 字节索引（自动生成）。",
        " *",
        " * 生成：scripts/gen_alg7_interleave_rom.py",
        " */",
        "#pragma once",
        "",
        "#include <cstdint>",
        "",
        "namespace F203Alg7 {",
        "",
        f"constexpr uint32_t kInterleaveStreamLen = {OUT_LEN}U;",
        f"constexpr uint32_t kInterleaveRomLen = {OUT_LEN}U;",
        "",
        "constexpr int32_t kAlg7InterleaveReorderByte[kInterleaveRomLen] = {",
    ]
    # 每行 8 个索引，便于 diff 阅读
    row = "    "
    for i, v in enumerate(idx):
        row += f"{v}, "
        if (i + 1) % 8 == 0:
            lines.append(row)
            row = "    "
    if row.strip():
        lines.append(row)
    lines.extend(
        [
            "};",
            "",
            "}  // namespace F203Alg7",
            "",
        ]
    )
    out_h.write_text("\n".join(lines), encoding="utf-8")
    print(f"[gen_alg7_interleave_rom] wrote {out_h} ({OUT_LEN} entries)")


if __name__ == "__main__":
    main()
