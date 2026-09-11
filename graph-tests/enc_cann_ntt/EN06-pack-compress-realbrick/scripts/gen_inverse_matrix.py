#!/usr/bin/python3
# coding=utf-8
"""
EN02 · ML-KEM inverse NTT 变换矩阵生成（供 Host 第二段 INTT launch）

只读改编自 thirdparty/cann-ntt/.../gen_pqc_data.py：
  - mlkem_inverse_ntt：逆向蝶形 + 前置缩放 512
  - build_transform_matrix：对标准基跑 transform 得 dense B
  - BAT / digit 打包思路：本刀核与 EN01 同构，workspace 吃的是
    作者包 M4 布局（4×[256×256] 的 7-bit digit 平面），
    而非 cann-ntt UT 的 parity-isolated 16 块 BAT。
    因此矩阵语义取自 mlkem_inverse_ntt，打包对齐 EN01 gen_data。

输出：numpy int32 矩阵 [256,256]；调用方再 digit-split 写 M4_intt.bin。
"""

from __future__ import annotations

N = 256
MLKEM_Q = 3329
MLKEM_QINV = 62209
# FIPS 203 / Kyber 参考 zetas（Montgomery 域表示）
MLKEM_ZETAS = (
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


def int16(value: int) -> int:
    """符号扩展到 int16 语义（与 gen_pqc_data 一致）。"""
    value &= 0xFFFF
    return value - 0x10000 if value >= 0x8000 else value


def mlkem_montgomery_reduce(value: int) -> int:
    """Montgomery 约化：把 Montgomery 域 zeta 拉回普通域系数。"""
    reduced = value & 0xFFFF
    inverted = (reduced * MLKEM_QINV) & 0xFFFF
    signed_inverted = int16(inverted)
    return int16((value - signed_inverted * MLKEM_Q) >> 16)


def mlkem_inverse_ntt(coeffs: list[int]) -> list[int]:
    """
    ML-KEM incomplete inverse NTT（n=256,q=3329）。
    前置 ×512：与 cann-ntt UT 一致，使 INTT∘NTT 回到原系数（差缩放已并入）。
    层序 layer=7..1，length=N>>layer，与正向 layer=1..7 对偶。
    """
    result = [(value % MLKEM_Q) * 512 % MLKEM_Q for value in coeffs]
    for layer in range(7, 0, -1):
        length = N >> layer
        root_index = (1 << layer) - 1
        for start in range(0, N, 2 * length):
            root = mlkem_montgomery_reduce(MLKEM_ZETAS[root_index]) % MLKEM_Q
            root_index -= 1
            for j in range(start, start + length):
                lhs = result[j]
                rhs = result[j + length]
                result[j] = (lhs + rhs) % MLKEM_Q
                result[j + length] = (rhs - lhs) * root % MLKEM_Q
    return result


def build_transform_matrix(transform) -> list[list[int]]:
    """
    构造 dense 变换矩阵 B：行向量多项式 a 经 a @ B 得到 transform(a)。
    对每个标准基 e_i 跑一遍 transform，拼成 256×256。
    """
    matrix: list[list[int]] = []
    for input_index in range(N):
        basis = [0] * N
        basis[input_index] = 1
        matrix.append(transform(basis))
    return matrix


def pack_m4_digit_planes(matrix_rows: list[list[int]]):
    """
    把 dense int 矩阵打成作者包 M4 四平面（每平面 n*n 个 int8，7-bit digit）。
    与 EN01 scripts/gen_data.py 一致：m0=(m>>0)&0x7f … m3=(m>>21)&0x7f。
    Kyber 路径核内只用 M0/M1；M2/M3 仍写出以保持 workspace 布局不变。
    """
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
    """对 bench 条 poly（flat int）逐条跑 mlkem_inverse_ntt，拼回 flat。"""
    import numpy as np

    out = []
    arr = np.asarray(coeffs_flat, dtype=np.int32).reshape(bench, n)
    for i in range(bench):
        out.append(mlkem_inverse_ntt([int(x) % q for x in arr[i].tolist()]))
    return np.array(out, dtype=np.int32).reshape(-1)


if __name__ == "__main__":
    # 自检：写 M4_intt 尺寸是否为 4*n*n
    m4 = pack_m4_digit_planes(build_transform_matrix(mlkem_inverse_ntt))
    assert len(m4) == 4 * N * N, len(m4)
    print(f"[OK] inverse M4 bytes={len(m4)}")
