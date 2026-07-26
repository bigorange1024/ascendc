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

    out_h = Path(__file__).resolve().parent.parent.parent / "prep" / "alg7" / "f203_alg7_deinterleave_rom.h"

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

    # 注：本头文件由本脚本每次 run.sh 重新生成并覆盖；若要调整文件头注释须改这里，
    #     直接手改生成后的 .h 文件会在下次 run.sh 时被覆盖丢失。
    body = [
        "/**",
        " * @file f203_alg7_deinterleave_rom.h",
        " * @brief Alg.7「xof 解交织」实验路径（F203_ALG7_D12_GATHER=1）只读索引表：",
        f" *        xof[{XOF_BYTES}B] 按每 3 字节一组的 (C0,C1,C2) → 三个 Gather 字节索引表（自动生成，禁止手改）。",
        " *",
        " * 表语义：",
        " *   - 设备侧先将 xof 的 672 个 uint8 逐字节零扩展进 `expanded[j]`（int32，j=0..671），",
        " *     使得可以用 AscendC::Gather（仅支持 int32、4 字节对齐偏移）按字节位置取值；",
        " *   - Alg.7 line 6 把 xof 每 3 字节视为一组三元组 (C0,C1,C2)：第 k 组（k=0..223）",
        " *     C0/C1/C2 分别位于 xof 字节下标 `3k`、`3k+1`、`3k+2`；",
        " *   - 三个表 `kAlg7DeinterleaveC0Byte/C1Byte/C2Byte[k]` 即该组 C0/C1/C2 在 `expanded`",
        " *     内的 **4 字节对齐偏移** = `4*(3k)`、`4*(3k+1)`、`4*(3k+2)`（expanded 元素本身是 int32，",
        " *     故字节偏移 = 元素下标×4）；",
        f" *   - 表长 `kDeinterleaveRomLen={NPAIRS}` 与候选对数 `kCandPairs` 一致，",
        f" *     `kDeinterleaveExpandedLen={XOF_BYTES}` 与 XOF 总字节数 `kXofBytes` 一致（均由",
        " *     `alg7_geom.CAND_PAIRS`/`XOF_BYTES` 同步）。",
        " *",
        " * 用法：仅 `F203_ALG7_D12_GATHER=1`（实验对照，生产默认 0）时生效——",
        " *   `f203_alg7_d12_vec.hpp::InitAlg7DeinterleaveRomUb` 把三表拷入 UB 索引张量 `idxC0/C1/C2`，",
        " *   `PackXofBytesToExpandedInt32` 完成零扩展，`DeinterleaveCandGatherFromUb` 用三次",
        " *   `AscendC::Gather` 一次性取出 c0/c1/c2[224]。生产路径（GATHER=0）不依赖本表，",
        " *   改用 `DeinterleaveCandScalarFromUb` 标量 `GetValue` 顺序拆字节（Phase2 tick 更优，见 STATUS.md）。",
        " *",
        " * 生成脚本：`python3 scripts/gen_alg7_deinterleave_rom.py`（纯算术推导，无随机性，可重复复现；",
        " * 若修改 `alg7_geom.CAND_PAIRS`/`XOF_BYTES` 须重新运行本脚本同步刷新本文件）。",
        " */",
        "#pragma once",
        "",
        "#include <cstdint>",
        "",
        "namespace F203Alg7 {",
        "",
        "/** 候选三元组个数（=XOF 总字节数/3），三张表的行数。 */",
        f"constexpr uint32_t kDeinterleaveRomLen = {NPAIRS}U;",
        "/** xof 逐字节零扩展后的 expanded 数组长度，等于 XOF 总字节数。 */",
        f"constexpr uint32_t kDeinterleaveExpandedLen = {NPAIRS * 3}U;",
        "",
    ]
    body.append("/** 第 k 组三元组的 C0 在 expanded 内的 4 对齐字节偏移 = 4*(3k)。 */")
    body.extend(emit_array("kAlg7DeinterleaveC0Byte", c0))
    body.append("")
    body.append("/** 第 k 组三元组的 C1 在 expanded 内的 4 对齐字节偏移 = 4*(3k+1)。 */")
    body.extend(emit_array("kAlg7DeinterleaveC1Byte", c1))
    body.append("")
    body.append("/** 第 k 组三元组的 C2 在 expanded 内的 4 对齐字节偏移 = 4*(3k+2)。 */")
    body.extend(emit_array("kAlg7DeinterleaveC2Byte", c2))
    body.extend(["", "}  // namespace F203Alg7", ""])

    out_h.write_text("\n".join(body), encoding="utf-8")
    print(f"[gen_alg7_deinterleave_rom] wrote {out_h} ({NPAIRS}×3 entries, 4-aligned)")


if __name__ == "__main__":
    main()
