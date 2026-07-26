#!/usr/bin/env python3
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

    out_h = Path(__file__).resolve().parent.parent.parent / "prep" / "alg7" / "f203_alg7_interleave_rom.h"
    # 注：本头文件由本脚本每次 run.sh 重新生成并覆盖；若要调整文件头注释须改这里，
    #     直接手改生成后的 .h 文件会在下次 run.sh 时被覆盖丢失。
    lines = [
        "/**",
        " * @file f203_alg7_interleave_rom.h",
        f" * @brief Alg.7 rej「交错」段只读索引表：d1[{NPAIRS}]‖d2[{NPAIRS}] scratch → stream[{OUT_LEN}] 的",
        " *        AscendC::Gather **源字节偏移**（自动生成，禁止手改）。",
        " *",
        " * 表语义：",
        " *   - rej 向量路径把剔除后的 d1'/d2'（各 224 个 int32）先 DataCopy 拼接为一段连续 scratch",
        " *     （布局 = d1'[0..223] || d2'[0..223]，字节基址依次为 0 与 224×4=896）；",
        " *   - FIPS 203 Alg.7 line 8–15 的规范扫描顺序是「同一三元组内先 d1 后 d2」，即",
        " *     stream[2k] = d1[k]，stream[2k+1] = d2[k]（k=0..223），共 kStreamLen=448 个 lane；",
        " *   - `kAlg7InterleaveReorderByte[m]` 即 stream 第 m 个 lane 对应 scratch 内的**字节偏移**",
        " *     （非元素下标）：偶数下标 m=2k → 值为 `4*k`（落在 d1 段）；",
        " *     奇数下标 m=2k+1 → 值为 `896+4*k`（落在 d2 段，896 = d2 相对 scratch 首地址的偏移）；",
        " *   - 表长 `kInterleaveRomLen = kInterleaveStreamLen = 448`，与 `f203_alg7_layout.h`",
        " *     的 `kStreamLen` 保持一致（由 `alg7_geom.STREAM_LEN` 同步）。",
        " *",
        " * 用法：Init 阶段由 `f203_alg7_rej_vec.hpp::InitAlg7InterleaveRomUb` 把本表逐元素拷入 UB",
        " * `idxRom` 张量（一次性、非热路径）；随后 `InterleaveD12GatherUb` 用",
        " * `AscendC::Gather(stream, scratchT1, idxRom, 0, kInterleaveStreamLen)` 一次性完成交错重排，",
        " * 避免逐 lane `GetValue`/`SetValue` 标量循环。",
        " *",
        " * 生成脚本：`python3 scripts/gen_alg7_interleave_rom.py`（纯算术推导，无随机性，可重复复现；",
        " * 若修改 `alg7_geom.CAND_PAIRS`/`STREAM_LEN` 须重新运行本脚本同步刷新本文件）。",
        " */",
        "#pragma once",
        "",
        "#include <cstdint>",
        "",
        "namespace F203Alg7 {",
        "",
        "/** 交错输出 stream 的 lane 总数：224 对 (d1,d2) × 2 = 448。 */",
        f"constexpr uint32_t kInterleaveStreamLen = {OUT_LEN}U;",
        "/** 本索引表长度，与 kInterleaveStreamLen 一一对应（每个 stream lane 一个 Gather 偏移）。 */",
        f"constexpr uint32_t kInterleaveRomLen = {OUT_LEN}U;",
        "",
        "/** stream[m] ← scratch 字节偏移 kAlg7InterleaveReorderByte[m]；偶 m 取自 d1 段，奇 m 取自 d2 段。 */",
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
