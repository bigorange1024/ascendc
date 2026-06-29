#!/usr/bin/env python3
# coding=utf-8
"""
mixPass=6 — AicMmad 隔离 sanity 数据（对照 tag5t planar-s12：16×256×128 单路 lo_even）。

用法：
  MMAD_SANITY_CASE=eye16 python3 scripts/gen_mmad_sanity_data.py
  MMAD_SANITY_CASE=scalar python3 scripts/gen_mmad_sanity_data.py

case 说明（C = A @ B，A=S0[16,256] int8，B=LUT_EVEN_TOP[256,128] int8）：
  eye16  — A、B 均为对角 1 → C 为 16×128 对角阵（一眼应见 C[i,i]=1）
  scalar — A[0,0]=3, B[0,0]=5 → C[0,0]=15，其余为 0
  row0   — A[0,j]=1 (j=0..255), B=I → C[0,j]=1 (j=0..127)，其余行 0
"""
from __future__ import annotations

import os

import numpy as np

N = 256
HALF_N = N // 2
SANITY_M = 16
LUT_STACKED_ROWS = 512
MIX_PASS = 6


def stacked_even_top(b_top: np.ndarray) -> np.ndarray:
    """[256,128] → lut_even_stacked.bin 布局（下 256 行填 0）。"""
    out = np.zeros((LUT_STACKED_ROWS, HALF_N), dtype=np.int8)
    out[:256, :] = b_top
    return out


def golden_c_lo_even(a: np.ndarray, b_top: np.ndarray) -> np.ndarray:
    return (a.astype(np.int32) @ b_top.astype(np.int32)).astype(np.int32)


def build_case(name: str) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    a = np.zeros((SANITY_M, N), dtype=np.int8)
    b_top = np.zeros((N, HALF_N), dtype=np.int8)

    if name == "eye16":
        for i in range(SANITY_M):
            a[i, i] = 1
            b_top[i, i] = 1
    elif name == "scalar":
        a[0, 0] = 3
        b_top[0, 0] = 5
    elif name == "row0":
        a[0, :] = 1
        for j in range(HALF_N):
            b_top[j, j] = 1
    else:
        raise SystemExit(f"[sanity] unknown MMAD_SANITY_CASE={name!r} (eye16|scalar|row0)")

    gold = golden_c_lo_even(a, b_top)
    return a, b_top, gold


def main() -> int:
    case = os.environ.get("MMAD_SANITY_CASE", "eye16")
    os.makedirs("input", exist_ok=True)
    os.makedirs("output", exist_ok=True)

    a, b_top, gold = build_case(case)
    lut_even = stacked_even_top(b_top)
    lut_odd = np.zeros_like(lut_even)

    buf = np.zeros(16, dtype=np.int32)
    buf[:3] = np.array([N, 4, MIX_PASS], dtype=np.int32)
    buf.tofile("./input/tiling.bin")

    a.tofile("./input/s0_sanity.bin")
    lut_even.tofile("./input/lut_even_stacked.bin")
    lut_odd.tofile("./input/lut_odd_stacked.bin")
    np.zeros((8, N), dtype=np.int32).tofile("./input/src.bin")
    np.zeros((16, N), dtype=np.int32).tofile("./input/a_hat.bin")

    gold.tofile("./output/golden_mat_c_tmp_lo_even.bin")
    a.tofile("./output/golden_s0_sanity.bin")

    nz = int(np.count_nonzero(gold))
    print(f"[sanity] case={case} mixPass={MIX_PASS} AicMmad({SANITY_M},{N},{HALF_N})")
    print(f"[sanity] golden C_lo_even shape={gold.shape} nonzero={nz}")
    if case == "eye16":
        diag = [int(gold[i, i]) for i in range(min(4, SANITY_M))]
        print(f"[sanity] expect diag C[i,i]=1, golden diag[0:4]={diag}")
    elif case == "scalar":
        print(f"[sanity] expect C[0,0]=15, golden C[0,0]={int(gold[0, 0])}")
    elif case == "row0":
        print(f"[sanity] expect C[0,0:4]=[1,1,1,1], golden={gold[0, :4].tolist()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
