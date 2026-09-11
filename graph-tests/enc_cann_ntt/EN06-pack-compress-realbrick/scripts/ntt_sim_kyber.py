#!/usr/bin/python3
# coding=utf-8

import os
import numpy as np
import math

# ============================================================
# 配置参数 - 8192点NTT
# ============================================================
NTT_N = 256

TOTAL_LENGTH = 256
TILE_NUM = 1
CORE_NUM = 8

logGatherDim = 1
logScatterDim = 2

# ============================================================
# 数学函数
# ============================================================
def mod_pow(base, exp, mod):
    result = 1
    base = base % mod
    while exp > 0:
        if exp & 1:
            result = (result * base) % mod
        exp >>= 1
        base = (base * base) % mod
    return result

def compute_omega(g, n, q):
    return pow(g, (q - 1) // n, q)

def kyber_ntt_matrix(zetas, n, q):
    M = np.zeros((n, n), dtype=int)
    for row in range(n):
        # 初始化一行单位向量
        e = np.zeros(n, dtype=int)
        e[row] = 1
        # 执行 Kyber 的 radix-2 butterfly NTT
        data = e.copy()
        zeta_index = 0
        length = n // 2
        while length >= 1:
            step = 2 * length
            for start in range(0, n, step):
                zeta = zetas[zeta_index]
                zeta_index += 1
                for j in range(start, start + length):
                    a = data[j]
                    b = data[j + length]
                    t = (zeta * b) % q
                    data[j] = (a + t) % q
                    data[j + length] = (a - t + q) % q
            length //= 2
        # 这一行就是矩阵 M 的 row 行
        M[row, :] = data
    return M

M = np.array([]) #type: np.ndarray

zetas = np.array([
  0,       4808194, 3765607, 3761513, 5178923, 5496691, 5234739, 5178987,
  7778734, 3542485, 2682288, 2129892, 3764867, 7375178, 557458,  7159240,
  5010068, 4317364, 2663378, 6705802, 4855975, 7946292, 676590,  7044481,
  5152541, 1714295, 2453983, 1460718, 7737789, 4795319, 2815639, 2283733,
  3602218, 3182878, 2740543, 4793971, 5269599, 2101410, 3704823, 1159875,
  394148,  928749,  1095468, 4874037, 2071829, 4361428, 3241972, 2156050,
  3415069, 1759347, 7562881, 4805951, 3756790, 6444618, 6663429, 4430364,
  5483103, 3192354, 556856,  3870317, 2917338, 1853806, 3345963, 1858416,
  3073009, 1277625, 5744944, 3852015, 4183372, 5157610, 5258977, 8106357,
  2508980, 2028118, 1937570, 4564692, 2811291, 5396636, 7270901, 4158088,
  1528066, 482649,  1148858, 5418153, 7814814, 169688,  2462444, 5046034,
  4213992, 4892034, 1987814, 5183169, 1736313, 235407,  5130263, 3258457,
  5801164, 1787943, 5989328, 6125690, 3482206, 4197502, 7080401, 6018354,
  7062739, 2461387, 3035980, 621164,  3901472, 7153756, 2925816, 3374250,
  1356448, 5604662, 2683270, 5601629, 4912752, 2312838, 7727142, 7921254,
  348812,  8052569, 1011223, 6026202, 4561790, 6458164, 6143691, 1744507,
  1753,    6444997, 5720892, 6924527, 2660408, 6600190, 8321269, 2772600,
  1182243, 87208,   636927,  4415111, 4423672, 6084020, 5095502, 4663471,
  8352605, 822541,  1009365, 5926272, 6400920, 1596822, 4423473, 4620952,
  6695264, 4969849, 2678278, 4611469, 4829411, 635956,  8129971, 5925040,
  4234153, 6607829, 2192938, 6653329, 2387513, 4768667, 8111961, 5199961,
  3747250, 2296099, 1239911, 4541938, 3195676, 2642980, 1254190, 8368000,
  2998219, 141835,  8291116, 2513018, 7025525, 613238,  7070156, 6161950,
  7921677, 6458423, 4040196, 4908348, 2039144, 6500539, 7561656, 6201452,
  6757063, 2105286, 6006015, 6346610, 586241,  7200804, 527981,  5637006,
  6903432, 1994046, 2491325, 6987258, 507927,  7192532, 7655613, 6545891,
  5346675, 8041997, 2647994, 3009748, 5767564, 4148469, 749577,  4357667,
  3980599, 2569011, 6764887, 1723229, 1665318, 2028038, 1163598, 5011144,
  3994671, 8368538, 7009900, 3020393, 3363542, 214880,  545376,  7609976,
  3105558, 7277073, 508145,  7826699, 860144,  3430436, 140244,  6866265,
  6195333, 3123762, 2358373, 6187330, 5365997, 6663603, 2926054, 7987710,
  8077412, 3531229, 4405932, 4606686, 1900052, 7598542, 1054478, 7648983
], dtype=np.int32)

def ntt_forward(coeffs, n, q):
    global M
    M = np.eye(n, dtype=int)

    data = np.array([c % q for c in coeffs], dtype=np.int32)
    # print(M)
    # print("zetas", len(zetas), zetas)
    zeta_index = 1
    length = n // 2
    while length >= 1:
        data_bak = data.copy()

        layer_mat = np.eye(n, dtype=np.int32)
        step = 2 * length
        for start in range(0, n, step):
            zeta = zetas[zeta_index]
            zeta_index += 1
            for j in range(start, start + length):
                a = data[j]
                b = data[j + length]
                t = (np.int64(zeta) * b) % q
                data[j] = (a + t) % q
                data[j + length] = (a - t + q) % q

                layer_mat[j, j] = 1
                layer_mat[j + length, j] = zeta
                layer_mat[j, j + length] = 1
                layer_mat[j + length, j + length] = q-zeta
        
        data_bak = (data_bak.astype(np.int64) @ layer_mat.astype(np.int64)) % q
        assert np.array_equal(data_bak, data)
        
        M = (M @ layer_mat) % q
        length //= 2
    return data

def ntt_test01(n, q, f: np.ndarray):
    return (f.astype(np.int64) @ M.astype(np.int64)) % q

def ntt_test01_nomod(n, q, g, f: np.ndarray):
    return (f.astype(np.int32) @ M.astype(np.int32))

def gen_golden_data(n, q, bench):

    print("=" * 60)
    print(f"NTT Test: N={n}, Q={q}")
    print("=" * 60)

    for i in range(bench):
        x  = np.random.randint(0, q, size=n, dtype=int) # type: np.ndarray[np.int64]
        z  = np.array(ntt_forward(x, n, q), dtype=np.int32)
        if i == 0:
            input_x = x; golden = z
        else:
            input_x = np.concatenate((input_x, x), dtype=np.int32)
            golden  = np.concatenate((golden, z),  dtype=np.int32)
    
    return (input_x, golden)
