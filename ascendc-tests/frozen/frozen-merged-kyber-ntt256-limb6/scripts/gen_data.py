#!/usr/bin/python3
# coding=utf-8
"""Phase D 6bit limb：M 按 6bit 切片生成 M4.bin，golden 仍为 f@M mod q。"""
import os
import sys

import numpy as np

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_SHARED = os.path.normpath(os.path.join(_SCRIPT_DIR, "../../../../library/shared"))
_NTT_SCRIPTS = os.path.normpath(
    os.path.join(_SCRIPT_DIR, "../../../../ascendc-tests/pass-merged-kyber-mix-ntt256/scripts")
)
sys.path.insert(0, _SHARED)
if os.path.isdir(_NTT_SCRIPTS):
    sys.path.insert(0, _NTT_SCRIPTS)

import merged_kyber_fixed_poly  # noqa: E402
import ntt_sim_kyber  # noqa: E402

N = 256
LIMB_BITS = 6
LIMB_MASK = (1 << LIMB_BITS) - 1


def gen_tiling():
    os.makedirs("input", exist_ok=True)
    os.makedirs("output", exist_ok=True)
    np.array([N], dtype=np.int32).tofile("./input/tiling.bin")


if __name__ == "__main__":
    gen_tiling()
    input_x = merged_kyber_fixed_poly.FIXED_POLY.copy()
    poly_out = ntt_sim_kyber.ntt_forward(input_x, N, 3329, 17)
    golden = np.array(poly_out, dtype=np.int32)

    input_x.tofile("./input/src.bin")
    m = ntt_sim_kyber.M.astype(np.int32)
    golden.tofile("./output/golden.bin")

    m0 = ((m >> 0) & LIMB_MASK).astype(np.int8).reshape(-1)
    m1 = ((m >> LIMB_BITS) & LIMB_MASK).astype(np.int8).reshape(-1)
    m2 = ((m >> (2 * LIMB_BITS)) & LIMB_MASK).astype(np.int8).reshape(-1)
    m3 = ((m >> (3 * LIMB_BITS)) & LIMB_MASK).astype(np.int8).reshape(-1)
    m_out = np.concatenate((m0, m1, m2, m3), dtype=np.int8)
    m_out.tofile("./input/M4.bin")

    test01 = ntt_sim_kyber.ntt_test01(n=N, q=3329, g=17, f=input_x)
    assert np.all(test01 == golden)

    print(f"[gen_data] fixed poly seed={merged_kyber_fixed_poly._SEED} md5={__import__('hashlib').md5(input_x.tobytes()).hexdigest()}")
    print(f"[gen_data] limb={LIMB_BITS}bit M4.bin bytes={m_out.nbytes}")
    print(f"[gen_data] m0 max={m0.max()} m1 max={m1.max()}")
