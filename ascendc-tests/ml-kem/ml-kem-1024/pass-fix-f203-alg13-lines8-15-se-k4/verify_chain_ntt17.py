#!/usr/bin/env python3
# coding=utf-8
"""对拍：链式 8–17 — Launch1 src + Launch2 NTT dst/s0/mat_c。"""
from __future__ import annotations

import struct
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent
N = 256
K_S = 4
K_E = 4
K_DST = 2 * K_S + K_E
M_ROWS = 32
HALF_N = N // 2
MAT_C_PLANAR_ROWS = 96


def check(label: str, got: np.ndarray, ref: np.ndarray) -> int:
    diff = np.abs(got.astype(np.int64) - ref.astype(np.int64))
    mx = int(diff.max())
    if mx != 0:
        flat = int(diff.argmax())
        print(f"[FAIL] {label} max_abs_diff={mx} flat_idx={flat}")
        print(f"  expected={int(ref.reshape(-1)[flat])} actual={int(got.reshape(-1)[flat])}")
        return 1
    print(f"[SUCCESS] {label} max_abs_diff=0")
    return 0


def main() -> int:
    rc = 0

    golden_src = np.fromfile(ROOT / "output" / "golden_src.bin", dtype=np.int32).reshape(8, N)
    got_src = np.fromfile(ROOT / "output" / "src.bin", dtype=np.int32).reshape(8, N)
    rc |= check("Launch1 src vs golden_src (Alg.13 8–15)", got_src, golden_src)

    if (ROOT / "output" / "golden_dst.bin").is_file():
        g_dst = np.fromfile(ROOT / "output" / "golden_dst.bin", dtype=np.int32).reshape(K_DST, N)
        d_dst = np.fromfile(ROOT / "output" / "dst.bin", dtype=np.int32).reshape(K_DST, N)
        rc |= check("Launch2 dst vs golden_dst (Alg.13 16–17)", d_dst, g_dst)

        s0 = d_dst[:K_S]
        s1 = d_dst[K_S : 2 * K_S]
        rc |= check("device s_hat_aiv0 vs s_hat_aiv1 (双 AIV 复制 ŝ)", s0, s1)
        rc |= check("s_hat_aiv0 vs golden ŝ", s0, g_dst[:K_S])

        e_got = d_dst[2 * K_S :]
        e_ref = g_dst[2 * K_S :]
        rc |= check("e_hat vs golden ê", e_got, e_ref)

    if (ROOT / "output" / "golden_s0.bin").is_file() and (ROOT / "output" / "s0.bin").is_file():
        g_s0 = np.fromfile(ROOT / "output" / "golden_s0.bin", dtype=np.int8).reshape(M_ROWS, N)
        s0 = np.fromfile(ROOT / "output" / "s0.bin", dtype=np.int8).reshape(M_ROWS, N)
        rc |= check("S0 vs golden_s0 (Stage1)", s0, g_s0)

    if (ROOT / "output" / "golden_mat_c.bin").is_file() and (ROOT / "output" / "mat_c.bin").is_file():
        g_mc = np.fromfile(ROOT / "output" / "golden_mat_c.bin", dtype=np.int32).reshape(MAT_C_PLANAR_ROWS, HALF_N)
        mc = np.fromfile(ROOT / "output" / "mat_c.bin", dtype=np.int32).reshape(MAT_C_PLANAR_ROWS, HALF_N)
        rc |= check("mat_c_planar vs golden (Stage2)", mc, g_mc)

    seed_d = struct.unpack("<I", (ROOT / "input" / "seed_d.bin").read_bytes())[0]
    if rc == 0:
        print(f"[verify_chain_ntt17] PASS SEED_D={seed_d}")
    else:
        print(f"[verify_chain_ntt17] FAIL SEED_D={seed_d}")
    return rc


if __name__ == "__main__":
    sys.exit(main())
