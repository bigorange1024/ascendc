#!/usr/bin/python3
# coding=utf-8
# EN02-ntt-intt-2launch · 双 launch 造数
# Launch1 正向 NTT：沿用 EN01 Kyber 路径（ntt_kyber）
# Launch2 逆向 INTT：inverse 矩阵/参考来自 gen_inverse_matrix（改编 cann-ntt UT）
# 写出：
#   input/tiling.bin
#   input/src_ntt.bin  input/M4_ntt.bin   output/golden_ntt.bin
#   input/src_intt.bin input/M4_intt.bin  output/golden_intt.bin

import numpy as np
import os
import ntt_kyber
import gen_inverse_matrix as inv

n = int(os.environ.get("NTT_N", "256"))
bench = int(os.environ.get("NTT_BENCH", "4"))
q = int(os.environ.get("NTT_Q", "3329"))
ref = os.environ.get("NTT_REF", "kyber").lower()
np.random.seed(99)


def barrett_params(modulus):
    """Barrett 参数 b_k / b_mu，写入 tiling 供核内约化。"""
    b_k = 0
    temp_q = int(modulus)
    while temp_q > 0:
        temp_q >>= 1
        b_k += 1
    b_mu = (1 << (2 * b_k)) // int(modulus)
    return b_k, b_mu


def pack_m4_from_dense(m: np.ndarray) -> np.ndarray:
    """dense int32 矩阵 → 作者包 M4 四平面 int8（与 EN01 一致）。"""
    m = m.astype(np.int32)
    m0 = ((m >> 0) & 0x7F).astype(np.int8).reshape(-1)
    m1 = ((m >> 7) & 0x7F).astype(np.int8).reshape(-1)
    m2 = ((m >> 14) & 0x7F).astype(np.int8).reshape(-1)
    m3 = ((m >> 21) & 0x7F).astype(np.int8).reshape(-1)
    return np.concatenate((m0, m1, m2, m3), dtype=np.int8)


def gen_tiling():
    os.makedirs("input", exist_ok=True)
    os.makedirs("output", exist_ok=True)
    b_k, b_mu = barrett_params(q)
    r28 = (1 << 28) % int(q)
    print(f"[INFO] tiling: n={n}, bench={bench}, q={q}, b_k={b_k}, b_mu={b_mu}, r28={r28}")
    tiling_np = np.array([n, bench, q, b_k, b_mu, r28], dtype=np.int32)
    tiling_np.tofile("./input/tiling.bin")


def gen_forward_ntt():
    """Launch1：时间域 src → 正向 NTT golden；M4_ntt 为正向 dense digit。"""
    if ref not in ("kyber", "ml-kem", "mlkem"):
        raise ValueError(f"EN02 锁定 NTT_REF=kyber，收到 {ref!r}")
    src, golden = ntt_kyber.gen_kyber_data(n=n, q=q, bench=bench)
    m4 = pack_m4_from_dense(ntt_kyber.M)
    src.astype(np.int32).tofile("./input/src_ntt.bin")
    golden.astype(np.int32).tofile("./output/golden_ntt.bin")
    m4.tofile("./input/M4_ntt.bin")
    print(f"[INFO] NTT: src={src.size} golden={golden.size} M4={m4.size}")


def gen_inverse_intt():
    """
    Launch2：NTT 域 src → INTT golden；M4_intt 由 mlkem_inverse_ntt 矩阵 digit 打包。
    输入独立随机（粘性测的是双 launch 不挂，不强制链式喂数）。
    """
    # 独立种子，避免与正向同一批偶然相关
    rng = np.random.RandomState(101)
    src_list = []
    golden_list = []
    for _ in range(bench):
        x = rng.randint(0, q, size=n, dtype=int)
        z = inv.mlkem_inverse_ntt([int(v) for v in x.tolist()])
        src_list.append(np.asarray(x, dtype=np.int32))
        golden_list.append(np.asarray(z, dtype=np.int32))
    src = np.concatenate(src_list)
    golden = np.concatenate(golden_list)
    m_inv = inv.build_mlkem_inverse_matrix_int32()
    m4 = pack_m4_from_dense(m_inv)
    src.tofile("./input/src_intt.bin")
    golden.tofile("./output/golden_intt.bin")
    m4.tofile("./input/M4_intt.bin")
    print(f"[INFO] INTT: src={src.size} golden={golden.size} M4={m4.size} "
          f"matrix_max={int(m_inv.max())}")


if __name__ == "__main__":
    gen_tiling()
    gen_forward_ntt()
    gen_inverse_intt()
    print("[OK] EN02 dual-launch inputs written")
