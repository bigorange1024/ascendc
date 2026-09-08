#!/usr/bin/python3
# coding=utf-8
# EN02（自 EN01）：Kyber/ML-KEM 正向 NTT 参考与变换矩阵（迁入 merged_dsa）

import numpy as np

KYBER_N = 256
KYBER_Q = 3329
KYBER_MONT = (1 << 16) % KYBER_Q
KYBER_MONT_INV = pow(KYBER_MONT, -1, KYBER_Q)

M = np.array([])

# Kyber/ML-KEM reference zetas in Montgomery representation.
_ZETAS_MONT = np.array([
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
], dtype=np.int32)

ZETAS = ((_ZETAS_MONT.astype(np.int64) * KYBER_MONT_INV) % KYBER_Q).astype(np.int32)


def kyber_ntt(coeffs, n=KYBER_N, q=KYBER_Q):
    if n != KYBER_N or q != KYBER_Q:
        raise ValueError("Kyber NTT requires n=256 and q=3329")

    data = np.array([int(c) % q for c in coeffs], dtype=np.int32)
    k = 1
    length = n // 2
    while length >= 2:
        step = 2 * length
        for start in range(0, n, step):
            zeta = int(ZETAS[k])
            k += 1
            for j in range(start, start + length):
                t = (zeta * int(data[j + length])) % q
                a = int(data[j])
                data[j] = (a + t) % q
                data[j + length] = (a - t) % q
        length //= 2
    return data


def kyber_ntt_matrix(n=KYBER_N, q=KYBER_Q):
    rows = []
    for row in range(n):
        e = np.zeros(n, dtype=np.int32)
        e[row] = 1
        rows.append(kyber_ntt(e, n, q))
    return np.array(rows, dtype=np.int32)


def gen_kyber_data(n, q, bench):
    global M

    print("=" * 60)
    print(f"Kyber incomplete NTT: N={n}, Q={q}, stop length=2")
    print("=" * 60)

    M = kyber_ntt_matrix(n, q)
    inputs = []
    goldens = []
    for _ in range(bench):
        x = np.random.randint(0, q, size=n, dtype=int)
        z = kyber_ntt(x, n, q)
        inputs.append(x.astype(np.int32))
        goldens.append(z.astype(np.int32))

    return (np.concatenate(inputs).astype(np.int32),
            np.concatenate(goldens).astype(np.int32))
