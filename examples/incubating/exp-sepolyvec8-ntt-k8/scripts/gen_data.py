#!/usr/bin/python3
# coding=utf-8
"""exp-sepolyvec8-ntt-k8：8 条互不相同随机 poly；golden 为逐行 NTT [8,256]。"""
import hashlib
import os
import sys

import numpy as np

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_MERGED_SCRIPTS = os.path.normpath(os.path.join(_SCRIPT_DIR, "../../../../thirdparty/merged_kyber/scripts"))
sys.path.insert(0, _MERGED_SCRIPTS)

import ntt_sim_kyber  # noqa: E402

N = 256
K_POLYS = 8
LIMB_BITS = 6
LIMB_MASK = (1 << LIMB_BITS) - 1
Q = 3329
G = 17
SEED = 20260610


def gen_distinct_random_polys(rng: np.random.Generator) -> np.ndarray:
    """Generate K_POLYS distinct coefficient vectors in Z_q."""
    polys = []
    seen = set()
    while len(polys) < K_POLYS:
        row = rng.integers(0, Q, size=N, dtype=np.int32)
        key = row.tobytes()
        if key in seen:
            continue
        seen.add(key)
        polys.append(row)
    return np.stack(polys, axis=0)


def gen_lut_m4() -> np.ndarray:
    m = ntt_sim_kyber.M.astype(np.int32)
    m0 = ((m >> 0) & LIMB_MASK).astype(np.int8).reshape(-1)
    m1 = ((m >> LIMB_BITS) & LIMB_MASK).astype(np.int8).reshape(-1)
    m2 = ((m >> (2 * LIMB_BITS)) & LIMB_MASK).astype(np.int8).reshape(-1)
    m3 = ((m >> (3 * LIMB_BITS)) & LIMB_MASK).astype(np.int8).reshape(-1)
    return np.concatenate((m0, m1, m2, m3), dtype=np.int8)


def golden_ntt_batch(src: np.ndarray) -> np.ndarray:
    golden = np.zeros((K_POLYS, N), dtype=np.int32)
    for p in range(K_POLYS):
        golden[p] = np.array(ntt_sim_kyber.ntt_forward(src[p], N, Q, G), dtype=np.int32)
        test01 = ntt_sim_kyber.ntt_test01(n=N, q=Q, g=G, f=src[p])
        assert np.all(test01 == golden[p]), f"ntt_test01 mismatch at poly {p}"
    return golden


def gen_tiling():
    os.makedirs("input", exist_ok=True)
    os.makedirs("output", exist_ok=True)
    np.array([N, K_POLYS], dtype=np.int32).tofile("./input/tiling.bin")


if __name__ == "__main__":
    gen_tiling()
    rng = np.random.default_rng(SEED)
    src_batch = gen_distinct_random_polys(rng)
    assert len({row.tobytes() for row in src_batch}) == K_POLYS

    golden = golden_ntt_batch(src_batch)
    src_batch.tofile("./input/se_polyvec_gm.bin")
    golden.tofile("./output/golden.bin")
    gen_lut_m4().tofile("./input/mat_b_lut_gm.bin")

    print(f"[gen_data] seed={SEED} distinct polys={K_POLYS}")
    print(f"[gen_data] se_polyvec_gm [{K_POLYS},{N}] int32 md5={hashlib.md5(src_batch.tobytes()).hexdigest()}")
    print(f"[gen_data] mat_b_lut_gm [{N},{4 * N}] int8 (M0..M3)")
    print(f"[gen_data] golden [{K_POLYS},{N}] int32 (per-row NTT)")
