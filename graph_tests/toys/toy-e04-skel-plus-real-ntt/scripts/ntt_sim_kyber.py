#!/usr/bin/python3
# coding=utf-8

import os
import numpy as np
import math

# ============================================================
# 配置参数 - 8192点NTT
# ============================================================
NTT_N = 256
NTT_Q = 3329
NTT_G = 17

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

def generate_zetas(g, n, q):
    omega = compute_omega(g, n, q)
    print(f"omega = {omega}")
    zetas = [0] * n
    zetas[0] = 1
    k = 1
    length = n // 2
    while length >= 1:
        w_base = mod_pow(omega, n // (2 * length), q)
        w = 1
        num_groups = n // (2 * length)
        for i in range(num_groups):
            if k < n: 
                zetas[k] = w
                k += 1
            w = (w * w_base) % q
        length //= 2
    return zetas

def ntt_forward(coeffs, n, q, g):
    global M
    M = np.eye(n, dtype=int)

    data = [c % q for c in coeffs]
    zetas = generate_zetas(g, n, q)
    # print(M)
    # print("zetas", len(zetas), zetas)
    zeta_index = 1
    length = n // 2
    while length >= 1:
        data_bak = data.copy()

        layer_mat = np.eye(n, dtype=int)
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

                layer_mat[j, j] = 1
                layer_mat[j + length, j] = zeta
                layer_mat[j, j + length] = 1
                layer_mat[j + length, j + length] = q-zeta
        
        data_bak = (data_bak @ layer_mat) % q
        assert np.array_equal(data_bak, data)
        
        M = (M @ layer_mat) % q
        length //= 2
    return data

def ntt_test01(n, q, g, f: np.ndarray):
    return (f.astype(np.int32) @ M.astype(np.int32)) % q

def ntt_test01_nomod(n, q, g, f: np.ndarray):
    return (f.astype(np.int32) @ M.astype(np.int32))

def ntt_test02(n, q, g, input_x):
    '''
    B. Polynomial Multiplication in Module-LWE
    2) Matrix-Based NTT
    '''    
    n1 = 16; n2 = 16
    print(f"{n1} * {n2} = {n}")

    psi  = compute_omega(g, n, q)
    psi1 = compute_omega(g, n1, q)
    psi2 = compute_omega(g, n2, q)

    W_N = np.zeros((n2, n1), dtype=np.int64)
    for x1 in range(n2):
        for y1 in range(n1):
            W_N[x1, y1] = pow(psi, x1 * y1, q)
    
    C_N2 = np.zeros((n2, n2), dtype=np.int64)
    for x1 in range(n2):
        for y2 in range(n2):
            C_N2[x1, y2] = pow(psi2, x1 * y2, q)
    
    F = input_x.reshape([n2, n1]).astype(np.int64)

    Y = (W_N * (C_N2 @ F)) % q
    C_N1 = np.zeros((n1, n1), dtype=np.int64)
    for x2 in range(n1):
        for y1 in range(n1):
            C_N1[x2, y1] = pow(psi1, x2 * y1, q)
    
    F_bar = (C_N1 @ Y.T) % q
    
    f_bar = F_bar.reshape(-1)
    return f_bar

def gen_golden_data(n, q, g):

    print("=" * 60)
    print(f"NTT Test: N={n}, Q={q}, G={g}")
    print("=" * 60)

    np.random.seed(42)
    input_x = np.random.randint(0, q, size=n, dtype=int) # type: np.ndarray[np.int64]

    poly_out = ntt_forward(input_x, n, q, g)
    golden = np.array(poly_out, dtype=np.int32)
    
    return (input_x, golden)

def bit_reverse(x, logn):
    r = 0
    for _ in range(logn):
        r = (r << 1) | (x & 1)
        x >>= 1
    return r

def reorder_bitrev(a):
    n = len(a)
    logn = n.bit_length() - 1
    b = [0]*n
    for i in range(n):
        b[bit_reverse(i, logn)] = a[i]
    return b

def main():
    # np.set_printoptions(threshold=np.inf, linewidth=np.inf)
    (input_x, golden) = gen_golden_data(NTT_N, NTT_Q, NTT_G)
    res01 = ntt_test01(NTT_N, NTT_Q, NTT_G, input_x)
    # res02 = ntt_test02(NTT_N, NTT_Q, NTT_G, input_x)
    # print('golden', golden[0:10])
    print('res01 ',  res01[0:10])
    # print('res02',  res02[0:10])
    # res03 = reorder_bitrev(golden)
    # res03 = np.array(res03)
    print('golden',  golden[0:10])

if __name__ == "__main__": 
    main()