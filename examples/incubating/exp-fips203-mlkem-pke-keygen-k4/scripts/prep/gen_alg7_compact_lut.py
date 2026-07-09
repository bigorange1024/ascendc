#!/usr/bin/env python3
# @probe exp-fips203-mlkem-pke-keygen-k4
# @file scripts/prep/gen_alg7_compact_lut.py
# @layer script
# @role 探针脚本：支持 golden、LUT 生成、验证或 SIM 编排。 / Helper script `gen_alg7_compact_lut.py`.
# @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
# @launch N/A（host / 脚本 / CMake 不参与 device launch）
# @ai_core N/A（非 AI Core 内核源）
# @depends Python3 标准库；可能 import 同目录 keygen_golden / numpy。
# @verify 随 run.sh 全链或子目录 run_orchestrated/sim_*.sh 验收。

# coding=utf-8
"""生成 8-lane accept mask → Gather 字节偏移 LUT → f203_alg7_compact_lut.h。

Alg.7 向量 rej compact 路径将 448 个 stream lane 按 8×int32 分块处理：
  1. Compares/Mins 对每 lane 做 v<q 判定，得到 8-bit accept mask（256 种可能）
  2. 根据 mask 中置位 lane，用 Gather 将**被接受**的系数紧凑写入输出缓冲

本脚本枚举 mask=0..255，对每个 mask：
  - kAlg7CompactMask8Count[mask] = popcount(mask)，即本块接受系数个数
  - kAlg7CompactMask8GatherByte[mask][*] = 被接受 lane 在 8×int32 tile 内的字节偏移
    （lane i 对应偏移 i×4；未用槽位填 0  padding 至 8 项）

常量 kAlg7CompactStreamChunks = 448/8 与 f203_alg7_layout.h 中 kStreamLen 一致。
"""
from __future__ import annotations

from pathlib import Path

OUT = Path(__file__).resolve().parents[2] / "prep" / "alg7" / "f203_alg7_compact_lut.h"
# 每个 compact 块处理的 int32 lane 数（与向量 Compares tile 宽度对齐）
LANES = 8


def main() -> None:
    counts: list[int] = []
    gather_rows: list[list[int]] = []
    # 穷举 8-bit accept mask：bit i 表示 lane i 是否通过 v<q 剔除
    for mask in range(256):
        idxs = [i for i in range(LANES) if (mask >> i) & 1]
        counts.append(len(idxs))
        # Gather 索引：接受 lane 在 tile 内的字节偏移（int32 步长 ×4）
        row = [i * 4 for i in idxs]
        while len(row) < LANES:
            row.append(0)  # 未用槽位填 0，保证 C++ 数组定长 [8]
        gather_rows.append(row)

    lines = [
        "/**",
        " * @file f203_alg7_compact_lut.h",
        " * @brief 8-lane accept mask → Gather 字节偏移（由 gen_alg7_compact_lut.py 生成）。",
        " */",
        "#pragma once",
        "",
        "#include <cstdint>",
        "",
        "namespace F203Alg7 {",
        "",
        f"constexpr uint32_t kAlg7CompactChunkLanes = {LANES}U;",
        f"constexpr uint32_t kAlg7CompactMaskLutLen = 256U;",
        f"constexpr uint32_t kAlg7CompactStreamChunks = 448U / {LANES}U;",
        "constexpr uint32_t kAlg7CompactCompareCount = 128U;",
        "constexpr uint32_t kAlg7CompactComparePad = kAlg7CompactCompareCount - kAlg7CompactChunkLanes;",
        "",
        "constexpr uint8_t kAlg7CompactMask8Count[kAlg7CompactMaskLutLen] = {",
        ", ".join(str(c) for c in counts),
        "};",
        "",
        "/** gatherIdx[k] = 接受 lane 在 8×int32 tile 内的字节偏移（Gather 索引）。 */",
        "constexpr int32_t kAlg7CompactMask8GatherByte[kAlg7CompactMaskLutLen][8] = {",
    ]
    for row in gather_rows:
        lines.append("    {" + ", ".join(str(x) for x in row) + "},")
    lines += [
        "};",
        "",
        "}  // namespace F203Alg7",
        "",
    ]
    OUT.write_text("\n".join(lines), encoding="utf-8")
    print(f"OK: {OUT}")


if __name__ == "__main__":
    main()
