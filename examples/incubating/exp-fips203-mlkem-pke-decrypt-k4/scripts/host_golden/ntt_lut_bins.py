#!/usr/bin/env python3
"""
ntt_lut_bins.py — Decrypt 静态 NTT/INTT LUT 落盘（与 Encrypt 探针同布局）。

从 transpose_mlkem_luts_i8.h 解析 T 矩阵，切 even/odd stacked，写入 input/：
  lut_even_stacked.bin / lut_odd_stacked.bin
  lut_intt_even_stacked.bin / lut_intt_odd_stacked.bin
与 seed 无关；设备 workspace 由 Host memcpy 装入固定偏移。
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

import numpy as np

N = 256
CASE = Path(__file__).resolve().parent.parent.parent
LUT_HDR = CASE / "compute/ntt_u/thirdparty/ntt_study/include/mlkem/stable/transpose_mlkem_luts_i8.h"


def load_lut_t_i8(mode: str) -> np.ndarray:
    """解析头文件中 kMlkemLimb6Ntt_T_i8 或 Intt 表 → [N,512] int8。"""
    symbol = "kMlkemLimb6Ntt_T_i8" if mode == "ntt" else "kMlkemLimb6Intt_T_i8"
    txt = LUT_HDR.read_text(encoding="utf-8")
    i0 = txt.index(symbol)
    i1 = txt.index("{", i0)
    i2 = txt.index("};", i1)
    nums = [int(x) for x in re.findall(r"-?\d+", txt[i1 + 1 : i2])]
    return np.array(nums, dtype=np.int8).reshape(N, 512)


def lut_planar_stacked(lut: np.ndarray, even: bool) -> np.ndarray:
    """按 even/odd 列切 top/bottom 半区后纵向拼接（设备 LUT_EVEN/ODD_STACKED 布局）。"""
    if even:
        top = lut[:, 0:N:2]
        bottom = lut[:, N:512:2]
    else:
        top = lut[:, 1:N:2]
        bottom = lut[:, N + 1 : 512 : 2]
    return np.concatenate([top, bottom], axis=0)


def main() -> None:
    out_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else CASE / "input"
    out_dir.mkdir(parents=True, exist_ok=True)
    lut_ntt = load_lut_t_i8("ntt")
    lut_intt = load_lut_t_i8("intt")
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
