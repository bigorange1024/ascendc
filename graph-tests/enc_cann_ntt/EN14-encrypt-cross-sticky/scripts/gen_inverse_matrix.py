#!/usr/bin/python3
# coding=utf-8
"""
EN13 · ML-KEM 逆向 NTT 变换矩阵（供 Host INTT launch）

相对 EN09 的修正（否则无法与 liboqs / FIPS Alg.10 对拍）：
  - 层序 length=2..128，ζ 下标从 127 递减（与正向 kyber_ntt 对偶）
  - 末尾 × 128^{-1} ≡ 3303 (mod q)，**不是** cann-ntt UT 的前置 ×512
  - 蝶形：t=a；a←a+b；b←ζ·(b−t)

自检：INTT(NTT(x)) ≡ x；与 Host 正向 ntt_kyber 互逆。
输出：dense int32 [256,256]；调用方 digit-split 写 M4_intt.bin。
"""

from __future__ import annotations

N = 256
MLKEM_Q = 3329
# 128^{-1} mod 3329（FIPS 203 Alg.10 / pqcrystals Kyber invNTT 末缩放）
MLKEM_INV_N_HALF = 3303

# 与 ntt_kyber._ZETAS_MONT 同表；此处先落到普通域（× 2^{-16}）
_ZETAS_MONT = (
    -1044, -758, -359, -1517, 1493, 1422, 287, 202,
    -171, 622, 1577, 182, 962, -1202, -1474, 1468,
    573, -1325, 264, 383, -829, 1458, -1602, -130,
    -681, 1017, 732, 608, -1542, 411, -205, -1571,
    1223, 652, -552, 1015, -1293, 1491, -282, -1544,
    516, -8, -320, -666, -1618, -1162, 126, 1469,
    -853, -90, -271, 830, 107, -1421, -247, -951,
    -398, 961, -1508, -725, 448, -1065, 677, -1275,
    -1103, 430, 555, 843, -1251, 871, 1550, 105,
    422, 587, 177, -235, -291, -460, 1574, 1653,
    -246, 778, 1159, -147, -777, 1483, -602, 1119,
    -1590, 644, -872, 349, 418, 329, -156, -75,
    817, 1097, 603, 610, 1322, -1285, -1465, 384,
    -1215, -136, 1218, -1335, -874, 220, -1187, -1659,
    -1185, -1530, -1278, 794, -1510, -854, -870, 478,
    -108, -308, 996, 991, 958, -1460, 1522, 1628,
)
_KYBER_MONT_INV = pow((1 << 16) % MLKEM_Q, -1, MLKEM_Q)
ZETAS = tuple((int(z) * _KYBER_MONT_INV) % MLKEM_Q for z in _ZETAS_MONT)


def mlkem_inverse_ntt(coeffs: list[int]) -> list[int]:
    """
    ML-KEM incomplete inverse NTT（n=256,q=3329），与 ntt_kyber.kyber_ntt 互逆。
    @param coeffs  NTT 域系数（长度 N）
    @return        普通域系数 ∈ [0,q)
    """
    data = [int(c) % MLKEM_Q for c in coeffs]
    k = 127
    length = 2
    while length <= 128:
        for start in range(0, N, 2 * length):
            zeta = int(ZETAS[k])
            k -= 1
            for j in range(start, start + length):
                t = data[j]
                b = data[j + length]
                data[j] = (t + b) % MLKEM_Q
                data[j + length] = (zeta * ((b - t) % MLKEM_Q)) % MLKEM_Q
        length *= 2
    f = MLKEM_INV_N_HALF
    return [(f * v) % MLKEM_Q for v in data]


def build_transform_matrix(transform) -> list[list[int]]:
    """对标准基 e_i 跑 transform，拼 dense B；行向量 a @ B = transform(a)。"""
    matrix: list[list[int]] = []
    for input_index in range(N):
        basis = [0] * N
        basis[input_index] = 1
        matrix.append(transform(basis))
    return matrix


def pack_m4_digit_planes(matrix_rows: list[list[int]]):
    """dense int 矩阵 → 作者包 M4 四平面 int8（7-bit digit）。"""
    import numpy as np

    m = np.array(matrix_rows, dtype=np.int32)
    m0 = ((m >> 0) & 0x7F).astype(np.int8).reshape(-1)
    m1 = ((m >> 7) & 0x7F).astype(np.int8).reshape(-1)
    m2 = ((m >> 14) & 0x7F).astype(np.int8).reshape(-1)
    m3 = ((m >> 21) & 0x7F).astype(np.int8).reshape(-1)
    return np.concatenate((m0, m1, m2, m3), dtype=np.int8)


def build_mlkem_inverse_matrix_int32():
    """返回 inverse NTT dense 矩阵 [256,256] int32。"""
    import numpy as np

    rows = build_transform_matrix(mlkem_inverse_ntt)
    return np.array(rows, dtype=np.int32)


def apply_inverse_ntt_batch(coeffs_flat, n: int = N, q: int = MLKEM_Q, bench: int = 1):
    """对 bench 条 poly（flat）逐条 INTT，拼回 flat。"""
    import numpy as np

    out = []
    arr = np.asarray(coeffs_flat, dtype=np.int32).reshape(bench, n)
    for i in range(bench):
        out.append(mlkem_inverse_ntt([int(x) % q for x in arr[i].tolist()]))
    return np.array(out, dtype=np.int32).reshape(-1)


if __name__ == "__main__":
    import numpy as np
    import ntt_kyber

    m4 = pack_m4_digit_planes(build_transform_matrix(mlkem_inverse_ntt))
    assert len(m4) == 4 * N * N, len(m4)
    rng = np.random.default_rng(0)
    x = rng.integers(0, MLKEM_Q, size=N, dtype=np.int32)
    y = ntt_kyber.kyber_ntt(x, n=N, q=MLKEM_Q)
    z = np.array(mlkem_inverse_ntt(y.tolist()), dtype=np.int32)
    assert np.array_equal(x, z), "INTT∘NTT roundtrip failed"
    # 矩阵布局：行向量 @ Minv
    Minv = build_mlkem_inverse_matrix_int32().astype(np.int64)
    z2 = (y.astype(np.int64) @ Minv) % MLKEM_Q
    assert np.array_equal(z2, x.astype(np.int64)), "Minv matrix mismatch"
    print(f"[OK] EN13 inverse M4 bytes={len(m4)}; INTT∘NTT roundtrip OK")
