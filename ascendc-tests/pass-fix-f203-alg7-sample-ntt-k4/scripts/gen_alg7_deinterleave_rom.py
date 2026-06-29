#!/usr/bin/env python3
# coding=utf-8
"""生成 Alg.7 xof[672] 三字节解交织 Gather 索引 ROM → f203_alg7_deinterleave_rom.h。

设备侧将 xof 展开为 uint8 数组后，按每 3 字节一组解出 d1/d2 需要分别 Gather c0、c1、c2 列：
  - 对第 k 组三元组，c0/c1/c2 在展开缓冲中的**字节下标**为 3*k, 3*k+1, 3*k+2
  - AscendC Gather(int32) 要求 srcOffset 为**4 对齐字节偏移**，故索引为 4×(3*k+δ)

本脚本生成三组各 NPAIRS=224 个索引表：
  kAlg7DeinterleaveC0Byte / C1Byte / C2Byte

供 f203_alg7_d12_vec.hpp 实验路径（F203_ALG7_D12_GATHER=1）使用；默认标量解交织不依赖此 ROM。
须与 alg7_geom.XOF_BYTES、CAND_PAIRS 及 f203_alg7_layout.h 同步。
"""
from __future__ import annotations

from pathlib import Path

from alg7_geom import CAND_PAIRS, XOF_BYTES

NPAIRS = CAND_PAIRS


def main() -> None:
    # expanded[j] = 将 byte j 零扩展为 int32；Gather 源偏移须 4 字节对齐
    c0 = [4 * (3 * k) for k in range(NPAIRS)]      # 每组第 0 字节
    c1 = [4 * (3 * k + 1) for k in range(NPAIRS)]  # 每组第 1 字节
    c2 = [4 * (3 * k + 2) for k in range(NPAIRS)]  # 每组第 2 字节

    out_h = Path(__file__).resolve().parent.parent / "f203_alg7_deinterleave_rom.h"

    def emit_array(name: str, vals: list[int]) -> list[str]:
        """将索引列表格式化为 C++ constexpr 数组行（每行 8 个元素）。"""
        lines = [f"constexpr int32_t {name}[kDeinterleaveRomLen] = {{"]
        row = "    "
        for i, v in enumerate(vals):
            row += f"{v}, "
            if (i + 1) % 8 == 0:
                lines.append(row)
                row = "    "
        if row.strip():
            lines.append(row)
        lines.append("};")
        return lines

    body = [
        "/**",
        " * @file f203_alg7_deinterleave_rom.h",
        f" * @brief Alg.7 xof {XOF_BYTES}B → c0/c1/c2[{NPAIRS}] Gather 字节索引（自动生成）。",
        " *",
        " * expanded[j]=byte j（int32）；索引为 expanded 内 4 对齐字节偏移。",
        " * 生成：scripts/gen_alg7_deinterleave_rom.py",
        " */",
        "#pragma once",
        "",
        "#include <cstdint>",
        "",
        "namespace F203Alg7 {",
        "",
        f"constexpr uint32_t kDeinterleaveRomLen = {NPAIRS}U;",
        f"constexpr uint32_t kDeinterleaveExpandedLen = {NPAIRS * 3}U;",
        "",
    ]
    body.extend(emit_array("kAlg7DeinterleaveC0Byte", c0))
    body.append("")
    body.extend(emit_array("kAlg7DeinterleaveC1Byte", c1))
    body.append("")
    body.extend(emit_array("kAlg7DeinterleaveC2Byte", c2))
    body.extend(["", "}  // namespace F203Alg7", ""])

    out_h.write_text("\n".join(body), encoding="utf-8")
    print(f"[gen_alg7_deinterleave_rom] wrote {out_h} ({NPAIRS}×3 entries, 4-aligned)")


if __name__ == "__main__":
    main()
