#!/usr/bin/env python3
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

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT.parent / "prep" / "alg7" / "f203_alg7_compact_lut.h"
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

    # 注：本头文件由本脚本每次 run.sh 重新生成并覆盖；若要调整文件头注释须改这里，
    #     直接手改生成后的 .h 文件会在下次 run.sh 时被覆盖丢失。
    lines = [
        "/**",
        " * @file f203_alg7_compact_lut.h",
        " * @brief Alg.7 rej **向量 compact（R5 实验路径）** 查表：8-lane accept mask → Gather 字节偏移。",
        " *",
        " * 表语义（由 scripts/gen_alg7_compact_lut.py 穷举生成，非手写）：",
        " *   - rej 剔除后的 stream[448]（拒绝 lane 已标为 q）按 8×int32 一个 chunk 处理；",
        " *     每 chunk 先用 AscendC::Compare(EQ, q) 得到「拒绝掩码」，取反得到 8-bit **accept mask**",
        " *     （bit i =1 表示 chunk 内第 i 个 lane 是「接受」系数，即 v<q）。",
        " *   - `kAlg7CompactMask8Count[mask]`：该 8-bit mask 对应 chunk 内被接受的系数个数",
        " *     = popcount(mask)，取值范围 [0,8]，供上层决定本 chunk 要 Gather 出多少个系数。",
        " *   - `kAlg7CompactMask8GatherByte[mask][0..7]`：该 mask 下，被接受的 lane 在",
        " *     「8×int32 tile」内的**字节偏移**（lane i 的偏移 = i×4），按 lane 序号从低到高排列；",
        " *     未用满的槽位（超出 popcount(mask) 的位置）填 0，仅为凑齐 C++ 定长数组 `[8]`，",
        " *     调用方须依据 `kAlg7CompactMask8Count[mask]` 只取前 `count` 个有效槽位，",
        " *     不能把填充的 0 当作「第 0 字节也被接受」误用。",
        " *   - 256 = 2^8，枚举全部 8-bit mask 取值，故表长 `kAlg7CompactMaskLutLen=256`。",
        " *",
        " * 用法：`f203_alg7_rej_compact.hpp::RejVecCompactStreamUb` 每 chunk 用 `accept8` 查",
        " * `kAlg7CompactMask8Count` 得本 chunk 接受数 `nTake`，再查 `kAlg7CompactMask8GatherByte[accept8]`",
        " * 填 Gather 索引，将接受系数紧凑写入输出缓冲；`kAlg7CompactStreamChunks=448/8=56` 为总 chunk 数。",
        " *",
        " * 现状（2026-06，见 STATUS.md §R5）：本表所属向量 compact 路线在 SIM 上 `Compare` 掩码读法",
        " * 未通过验收，生产默认走标量 compact（`f203_alg7_rej_scalar.hpp::RejScalarCompactStreamUb`）；",
        " * 本表与 `RejVecCompactStreamUb` 仅作 NPU/非 CPU 孪生下的实验对照，不在默认路径调用。",
        " *",
        " * 重新生成：`python3 scripts/gen_alg7_compact_lut.py`（纯枚举，无随机性，可重复复现）。",
        " */",
        "#pragma once",
        "",
        "#include <cstdint>",
        "",
        "namespace F203Alg7 {",
        "",
        "/** 每个 compact chunk 处理的 int32 lane 数（与 Compare tile 的「有效数据」宽度一致）。 */",
        f"constexpr uint32_t kAlg7CompactChunkLanes = {LANES}U;",
        "/** LUT 行数：8-bit accept mask 全枚举 = 2^8。 */",
        f"constexpr uint32_t kAlg7CompactMaskLutLen = 256U;",
        "/** stream[448] 按 8-lane 一组切分的 chunk 总数（448/8=56），须与 kStreamLen 同步。 */",
        f"constexpr uint32_t kAlg7CompactStreamChunks = 448U / {LANES}U;",
        "/** AscendC::Compare 单次调用要求的最小 lane 数（int32 Level-2 API 约束，非本表自定义）。 */",
        "constexpr uint32_t kAlg7CompactCompareCount = 128U;",
        "/** Compare tile 中「有效 8 lane 之外」需要 pad 的哑元 lane 数：128-8=120。 */",
        "constexpr uint32_t kAlg7CompactComparePad = kAlg7CompactCompareCount - kAlg7CompactChunkLanes;",
        "",
        "/** kAlg7CompactMask8Count[mask] = popcount(mask)，即该 8-bit accept mask 对应的接受系数个数。 */",
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
