#!/usr/bin/python3
# coding=utf-8
# EN01-kem256-ntt-port · golden / 输入生成
# 迁入自 thirdparty/cann-ntt-author-merged_dsa/scripts/gen_data.py
# 本刀默认锁 Kyber：NTT_Q=3329、NTT_REF=kyber、NTT_N=256（可由环境覆盖）

import numpy as np
import os
import ntt_sim_kyber
import ntt_reference
import ntt_kyber

n = int(os.environ.get("NTT_N", "256"))
bench = int(os.environ.get("NTT_BENCH", "4"))
q = int(os.environ.get("NTT_Q", "3329"))
ref = os.environ.get("NTT_REF", "kyber").lower()
compare_standard = os.environ.get(
    "NTT_COMPARE_STANDARD",
    os.environ.get("NTT_COMPARE_SYMPY", "0")
) == "1"
compare_cases = int(os.environ.get(
    "NTT_COMPARE_CASES",
    os.environ.get("NTT_COMPARE_STANDARD_CASES", "4")
))
np.random.seed(99)

def barrett_params(q):
    b_k = 0
    temp_q = int(q)
    while temp_q > 0:
        temp_q >>= 1
        b_k += 1
    b_mu = (1 << (2 * b_k)) // int(q)
    return b_k, b_mu

def gen_tiling():
    os.system("mkdir -p input")
    os.system("mkdir -p output")
    b_k, b_mu = barrett_params(q)
    r28 = (1 << 28) % int(q)
    print(f"[INFO] tiling: n={n}, bench={bench}, q={q}, b_k={b_k}, b_mu={b_mu}, r28={r28}")
    tiling_data = [n, bench, q, b_k, b_mu, r28]
    tiling_np = np.array(tiling_data, dtype=np.int32)
    tiling_np.tofile("./input/tiling.bin")


if __name__ == "__main__":
    gen_tiling()
    if ref in ("sympy", "standard", "standard_ntt", "cyclic"):
        (input_x, golden) = ntt_reference.gen_standard_data(n=n, q=q, bench=bench)
        m = ntt_reference.M.astype(np.int32)
    elif ref in ("kyber", "ml-kem", "mlkem"):
        (input_x, golden) = ntt_kyber.gen_kyber_data(n=n, q=q, bench=bench)
        m = ntt_kyber.M.astype(np.int32)
    elif ref in ("dilithium", "ml-dsa", "mldsa", "default"):
        (input_x, golden) = ntt_sim_kyber.gen_golden_data(n=n, q=q, bench=bench)
        m = ntt_sim_kyber.M.astype(np.int32)
    else:
        raise ValueError(f"unknown NTT_REF={ref!r}; use 'dilithium', 'standard', or 'kyber'")

    input_x = input_x.astype(np.int32)
    golden  = golden.astype(np.int32)
    if compare_standard:
        ntt_reference.compare_with_standard(input_x, golden, n, q, compare_cases)

    input_x.tofile("./input/src.bin")
    golden.tofile("./output/golden.bin")

    print(m)
    m0 = ((m >> 0 ) & 0x7f).astype(np.int8).reshape(-1)
    m1 = ((m >> 7 ) & 0x7f).astype(np.int8).reshape(-1)
    m2 = ((m >> 14) & 0x7f).astype(np.int8).reshape(-1)
    m3 = ((m >> 21) & 0x7f).astype(np.int8).reshape(-1)
    print(np.max(m0), np.max(m1), np.max(m2), np.max(m3))
    m_out = np.concatenate((m0, m1, m2, m3), dtype=np.int8)
    print("m_out size =", len(m_out))
    print(m_out)
    m_out.tofile("./input/M4.bin")

    # test01 = ntt_sim_kyber.ntt_test01(n=n, q=3329, g=17, f=input_x)
    # assert np.all(test01 == golden)

    # nomod = ntt_sim_kyber.ntt_test01_nomod(n=n, q=3329, g=17, f=input_x).astype(np.int32)
    # nomod.tofile("./output/nomod.bin")
