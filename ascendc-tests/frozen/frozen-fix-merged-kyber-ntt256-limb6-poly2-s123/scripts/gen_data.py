#!/usr/bin/python3
# coding=utf-8
"""poly2 s123：2×同 poly；golden 为 NTT [2,256]，与 limb6 单 poly golden 逐元素一致。"""
import os
import sys

import numpy as np

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_SHARED = os.path.normpath(os.path.join(_SCRIPT_DIR, "../../../../library/shared"))
sys.path.insert(0, _SHARED)
sys.path.insert(0, _SCRIPT_DIR)

import merged_kyber_fixed_poly  # noqa: E402
import ntt_sim_kyber  # noqa: E402

N = 256
K_POLYS = 2
LIMB_BITS = 6
LIMB_MASK = (1 << LIMB_BITS) - 1
Q = 3329
G = 17


def gen_tiling():
    os.makedirs("input", exist_ok=True)
    os.makedirs("output", exist_ok=True)
    np.array([N, K_POLYS], dtype=np.int32).tofile("./input/tiling.bin")


if __name__ == "__main__":
    gen_tiling()
    input_x = merged_kyber_fixed_poly.FIXED_POLY.copy()
    poly_ntt = np.array(ntt_sim_kyber.ntt_forward(input_x, N, Q, G), dtype=np.int32)
    golden = np.tile(poly_ntt.reshape(1, -1), (K_POLYS, 1))

    src_batch = np.tile(input_x.reshape(1, -1), (K_POLYS, 1))
    src_batch.tofile("./input/src.bin")
    golden.tofile("./output/golden.bin")

    m = ntt_sim_kyber.M.astype(np.int32)
    m0 = ((m >> 0) & LIMB_MASK).astype(np.int8).reshape(-1)
    m1 = ((m >> LIMB_BITS) & LIMB_MASK).astype(np.int8).reshape(-1)
    m2 = ((m >> (2 * LIMB_BITS)) & LIMB_MASK).astype(np.int8).reshape(-1)
    m3 = ((m >> (3 * LIMB_BITS)) & LIMB_MASK).astype(np.int8).reshape(-1)
    m_out = np.concatenate((m0, m1, m2, m3), dtype=np.int8)
    m_out.tofile("./input/M4.bin")

    test01 = ntt_sim_kyber.ntt_test01(n=N, q=Q, g=G, f=input_x)
    assert np.all(test01 == poly_ntt)

    print(f"[gen_data] fixed poly seed={merged_kyber_fixed_poly._SEED} md5={__import__('hashlib').md5(input_x.tobytes()).hexdigest()}")
    print(f"[gen_data] src [{K_POLYS},{N}] int32")
    print(f"[gen_data] golden [{K_POLYS},{N}] int32 (NTT, same as limb6 per row)")
