#!/usr/bin/env python3
"""
ntt_lut_bins.py — 写出设备 NTT/INTT 静态 LUT stacked bin（与 Encrypt 探针同布局）。

流水线位置：gen_data 装入 input/；Host H2D 进 nttWs/inttWs。
布局：从 thirdparty 头文件读 [256,512] int8 T 矩阵，按偶/奇列拆成
  top(半列) ‖ bottom(半列) → [512,128] int8，供 AIC MMAD 条带消费。
与 golden 关系：LUT 与 seed 无关；仅保证设备与 Host Stage123 用同一表。
本脚本不改 thirdparty 源，只读头文件常量。
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

import numpy as np

N = 256
CASE = Path(__file__).resolve().parent.parent.parent
# 只读：禁止改 vendor/thirdparty；此处仅解析已嵌入的 LUT 字面量
LUT_HDR = CASE / "compute/ntt_u/thirdparty/ntt_onnx/include/mlkem/stable/transpose_mlkem_luts_i8.h"


def load_lut_t_i8(mode: str) -> np.ndarray:
    """
    从 LUT 头文件解析 NTT 或 INTT 的 T 矩阵。

    @param mode "ntt" → kMlkemLimb6Ntt_T_i8；"intt" → kMlkemLimb6Intt_T_i8
    @return shape (256, 512) int8
    """
    symbol = "kMlkemLimb6Ntt_T_i8" if mode == "ntt" else "kMlkemLimb6Intt_T_i8"
    txt = LUT_HDR.read_text(encoding="utf-8")
    i0 = txt.index(symbol)
    i1 = txt.index("{", i0)
    i2 = txt.index("};", i1)
    nums = [int(x) for x in re.findall(r"-?\d+", txt[i1 + 1 : i2])]
    return np.array(nums, dtype=np.int8).reshape(N, 512)


def lut_planar_stacked(lut: np.ndarray, even: bool) -> np.ndarray:
    """
    将 [256,512] 拆成 even/odd 列的 top‖bottom stacked。

    even=True：偶列（0,2,…）的前半/后半；False：奇列。
    @return shape (512, 128) int8
    """
    if even:
        top = lut[:, 0:N:2]
        bottom = lut[:, N:512:2]
    else:
        top = lut[:, 1:N:2]
        bottom = lut[:, N + 1 : 512 : 2]
    return np.concatenate([top, bottom], axis=0)


def main() -> None:
    """写出 lut_{even,odd,intt_even,intt_odd}_stacked.bin 到 out_dir。"""
    out_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else CASE / "input"
    out_dir.mkdir(parents=True, exist_ok=True)
    lut_ntt = load_lut_t_i8("ntt")
    lut_intt = load_lut_t_i8("intt")
    # 四份 stacked：NTT 偶/奇 + INTT 偶/奇（设备 workspace 各装一份）
    lut_even = lut_planar_stacked(lut_ntt, True)
    lut_odd = lut_planar_stacked(lut_ntt, False)
    lut_intt_even = lut_planar_stacked(lut_intt, True)
    lut_intt_odd = lut_planar_stacked(lut_intt, False)
    lut_even.tofile(out_dir / "lut_even_stacked.bin")
    lut_odd.tofile(out_dir / "lut_odd_stacked.bin")
    lut_intt_even.tofile(out_dir / "lut_intt_even_stacked.bin")
    lut_intt_odd.tofile(out_dir / "lut_intt_odd_stacked.bin")
    print(f"[ntt_lut_bins] -> {out_dir}")


if __name__ == "__main__":
    main()
