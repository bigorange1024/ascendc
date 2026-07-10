#!/usr/bin/env python3
# coding=utf-8
"""
@file gen_data.py
@brief pass-fix-f203-alg14-lines2-24-encrypt-compute-tail-k4 — golden 生成。

流水线与设备语义对齐：
  1. 行 2/16–21：与 compute 探针相同（host 预置 y/a_hat/e1/e2/m/ek）
  2. e₂+=μ 折叠：golden 在 INTT 前对 e₂ 加 μ(m)，与 SIM 融合核前缀一致
  3. 行 22–24：Compress + ByteEncode → golden/c.bin（1568B = c₁‖c₂）

输出：input/*、golden/u.bin、golden_v.bin、golden/c.bin；非设备实现规格。
"""
from __future__ import annotations

import os
import re
import sys

import numpy as np

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_CASE_DIR = os.path.normpath(os.path.join(_SCRIPT_DIR, ".."))
sys.path.insert(0, _SCRIPT_DIR)
from mlkem_ref import stage31_mod  # noqa: E402

_NTT_LUT_HDR = os.path.normpath(
    os.path.join(_CASE_DIR, "../../thirdparty/ntt_study/include/mlkem/stable/transpose_mlkem_luts_i8.h")
)

N = 256
HALF_N = N // 2
K = 4
K_P = 5
K_INTT_PAD = 8


def m_rows_for_k(k_batch: int) -> int:
    """Stage1 S0 行数 = 2 * k_batch。"""
    return 2 * k_batch


def mat_c_planar_rows_for_k(k_batch: int) -> int:
    return k_batch * LIMBS * 2


def encode_compact(batch: np.ndarray, s0: np.ndarray, k_batch: int) -> None:
    for lp in range(batch.shape[0]):
        for r in range(N):
            v = int(batch[lp, r]) % Q
            s0[lp, r] = (v >> LIMB_BITS) & LIMB_MASK
            s0[k_batch + lp, r] = v & LIMB_MASK


def encode_s0(polys: np.ndarray, k_batch: int) -> np.ndarray:
    s0 = np.zeros((m_rows_for_k(k_batch), N), dtype=np.int8)
    encode_compact(polys, s0, k_batch)
    return s0

K_PER_AIV = 2
M_ROWS = 2 * K
LIMBS = 4
MAT_C_PLANAR_ROWS = K * LIMBS * 2
LIMB_MASK = 0x3F
LIMB_BITS = 6
Q = 3329
SEED = 20260708
MSG_BYTES = 32
HALF_Q = (Q + 1) // 2
C1_POLY_BYTES = 352
C1_BYTES = K * C1_POLY_BYTES
C2_BYTES = 160
C_BYTES = C1_BYTES + C2_BYTES


def mu_embed_from_m(m: bytes) -> np.ndarray:
    """行 20：m[32] → μ[256]，bit=1 → HALF_Q，与设备 mu_embed_from_message_ub 一致。"""
    out = np.zeros(N, dtype=np.int32)
    for c in range(N):
        i, j = c // 8, c % 8
        bit = (m[i] >> j) & 1
        out[c] = HALF_Q if bit else 0
    return out


def compress_d_scalar(u: int, d: int) -> int:
    """Compress_d 标量参考（d=5 Barrett / d=11 定点）。"""
    u = int(u) % Q
    if d == 5:
        d0 = u * 1290176
        return ((d0 + (1 << 26)) >> 27) & 0x1F
    if d == 11:
        d0 = u * 5284526080
        d0 = (d0 + (1 << 32)) >> 33
        return int(d0 & 0x7FF)
    raise ValueError(f"unsupported d={d}")


def byte_encode_d(F: np.ndarray, d: int) -> bytes:
    """ByteEncode_d：逐系数 LSB-first 比特流打包。"""
    bits: list[int] = []
    mask = (1 << d) - 1
    for val in F:
        a = int(val) & mask
        for j in range(d):
            bits.append(a & 1)
            a >>= 1
    out = bytearray((len(bits) + 7) // 8)
    for i, b in enumerate(bits):
        if b:
            out[i >> 3] |= 1 << (i & 7)
    return bytes(out)


def pack_ciphertext(u: np.ndarray, v: np.ndarray) -> bytes:
    """行 22–24：c = ByteEncode₁₁(Compress₁₁(u)) ‖ ByteEncode₅(Compress₅(v))，1568B。"""
    c1 = bytearray(C1_BYTES)
    for p in range(K):
        comp = np.array([compress_d_scalar(int(x), 11) for x in u[p]], dtype=np.int32)
        c1[p * C1_POLY_BYTES : (p + 1) * C1_POLY_BYTES] = byte_encode_d(comp, 11)
    comp_v = np.array([compress_d_scalar(int(x), 5) for x in v], dtype=np.int32)
    c2 = byte_encode_d(comp_v, 5)
    return bytes(c1) + c2

K_ALG11_GAMMAS = [
    17, 3312, 2761, 568, 583, 2746, 2649, 680, 1637, 1692, 723, 2606, 2288, 1041, 1100, 2229,
    1409, 1920, 2662, 667, 3281, 48, 233, 3096, 756, 2573, 2156, 1173, 3015, 314, 3050, 279,
    1703, 1626, 1651, 1678, 2789, 540, 1789, 1540, 1847, 1482, 952, 2377, 1461, 1868, 2687, 642,
    939, 2390, 2308, 1021, 2437, 892, 2388, 941, 733, 2596, 2337, 992, 268, 3061, 641, 2688,
    1584, 1745, 2298, 1031, 2037, 1292, 3220, 109, 375, 2954, 2549, 780, 2090, 1239, 1645, 1684,
    1063, 2266, 319, 3010, 2773, 556, 757, 2572, 2099, 1230, 561, 2768, 2466, 863, 2594, 735,
    2804, 525, 1092, 2237, 403, 2926, 1026, 2303, 1143, 2186, 2150, 1179, 2775, 554, 886, 2443,
    1722, 1607, 1212, 2117, 1874, 1455, 1029, 2300, 2110, 1219, 2935, 394, 885, 2444, 2154, 1175,
]


def load_lut_t_i8(mode: str) -> np.ndarray:
    symbol = "kMlkemLimb6Ntt_T_i8" if mode == "ntt" else "kMlkemLimb6Intt_T_i8"
    with open(_NTT_LUT_HDR, encoding="utf-8") as f:
        txt = f.read()
    i0 = txt.index(symbol)
    i1 = txt.index("{", i0)
    i2 = txt.index("};", i1)
    body = txt[i1 + 1 : i2]
    nums = [int(x) for x in re.findall(r"-?\d+", body)]
    expect = N * 512
    if len(nums) != expect:
        raise SystemExit(f"LUT {symbol} size {len(nums)} != {expect}")
    return np.array(nums, dtype=np.int8).reshape(N, 512)


def lut_planar_stacked(lut: np.ndarray, even: bool) -> np.ndarray:
    if even:
        top = lut[:, 0:N:2]
        bottom = lut[:, N:512:2]
    else:
        top = lut[:, 1:N:2]
        bottom = lut[:, N + 1 : 512 : 2]
    return np.concatenate([top, bottom], axis=0)


def encode_compact_k4(batch: np.ndarray, s0: np.ndarray) -> None:
    encode_compact(batch, s0, K)


def encode_s0_k4(polys: np.ndarray) -> np.ndarray:
    return encode_s0(polys, K)


def mat_c_tmp_golden(s0: np.ndarray, lut: np.ndarray):
    le = lut[:, 0:N:2]
    lo = lut[:, 1:N:2]
    he = lut[:, N:512:2]
    ho = lut[:, N + 1 : 512 : 2]
    c_lo_even = (s0.astype(np.int32) @ le.astype(np.int32)).astype(np.int32)
    c_lo_odd = (s0.astype(np.int32) @ lo.astype(np.int32)).astype(np.int32)
    c_hi_even = (s0.astype(np.int32) @ he.astype(np.int32)).astype(np.int32)
    c_hi_odd = (s0.astype(np.int32) @ ho.astype(np.int32)).astype(np.int32)
    return c_lo_even, c_lo_odd, c_hi_even, c_hi_odd


def planar_row_k(k_batch: int, slot: int, limb: int, half: int) -> int:
    return half * (k_batch * LIMBS) + slot * LIMBS + limb


def pack_bank_planar_k(
    c_lo_even, c_lo_odd, c_hi_even, c_hi_odd, k_batch: int, poly_base: int, k_polys: int, out: np.ndarray
) -> None:
    for lp in range(k_polys):
        hi_r = poly_base + lp
        lo_r = k_batch + poly_base + lp
        slot = poly_base + lp
        out[planar_row_k(k_batch, slot, 0, 0), :] = c_lo_even[hi_r, :]
        out[planar_row_k(k_batch, slot, 1, 0), :] = c_lo_odd[hi_r, :]
        out[planar_row_k(k_batch, slot, 2, 0), :] = c_lo_even[lo_r, :]
        out[planar_row_k(k_batch, slot, 3, 0), :] = c_lo_odd[lo_r, :]
        out[planar_row_k(k_batch, slot, 0, 1), :] = c_hi_even[hi_r, :]
        out[planar_row_k(k_batch, slot, 1, 1), :] = c_hi_odd[hi_r, :]
        out[planar_row_k(k_batch, slot, 2, 1), :] = c_hi_even[lo_r, :]
        out[planar_row_k(k_batch, slot, 3, 1), :] = c_hi_odd[lo_r, :]


def pack_mat_c_planar_k(c_lo_even, c_lo_odd, c_hi_even, c_hi_odd, k_batch: int) -> np.ndarray:
    k_per_aiv = k_batch // 2
    out = np.zeros((mat_c_planar_rows_for_k(k_batch), HALF_N), dtype=np.int32)
    pack_bank_planar_k(c_lo_even, c_lo_odd, c_hi_even, c_hi_odd, k_batch, 0, k_per_aiv, out)
    pack_bank_planar_k(c_lo_even, c_lo_odd, c_hi_even, c_hi_odd, k_batch, k_per_aiv, k_per_aiv, out)
    return out


def merge_planar_poly_k(mat_planar: np.ndarray, k_batch: int, slot: int) -> np.ndarray:
    hh = mat_planar[planar_row_k(k_batch, slot, 0, 0)].astype(np.int64)
    lh = mat_planar[planar_row_k(k_batch, slot, 1, 0)].astype(np.int64)
    hl = mat_planar[planar_row_k(k_batch, slot, 2, 0)].astype(np.int64)
    ll = mat_planar[planar_row_k(k_batch, slot, 3, 0)].astype(np.int64)
    raw_lo = hh * 4096 + (hl + lh) * 64 + ll
    hh = mat_planar[planar_row_k(k_batch, slot, 0, 1)].astype(np.int64)
    lh = mat_planar[planar_row_k(k_batch, slot, 1, 1)].astype(np.int64)
    hl = mat_planar[planar_row_k(k_batch, slot, 2, 1)].astype(np.int64)
    ll = mat_planar[planar_row_k(k_batch, slot, 3, 1)].astype(np.int64)
    raw_hi = hh * 4096 + (hl + lh) * 64 + ll
    out = np.zeros(N, dtype=np.int32)
    out[:HALF_N] = stage31_mod(raw_lo.astype(np.int32))
    out[HALF_N:] = stage31_mod(raw_hi.astype(np.int32))
    return out


def stage123_transform(polys: np.ndarray, mode: str, k_batch: int | None = None) -> np.ndarray:
    k_batch = K if k_batch is None else k_batch
    lut = load_lut_t_i8(mode)
    s0 = encode_s0(polys, k_batch)
    c_le, c_lo, c_he, c_ho = mat_c_tmp_golden(s0, lut)
    mat_planar = pack_mat_c_planar_k(c_le, c_lo, c_he, c_ho, k_batch)
    dst = np.zeros((k_batch, N), dtype=np.int32)
    for slot in range(k_batch):
        dst[slot] = merge_planar_poly_k(mat_planar, k_batch, slot)
    return dst


def barrett_red_coeff(x: int) -> int:
    q = Q
    t = x + (q & (x >> 31))
    t1 = (t * 78) >> 18
    x = t - t1 * q
    t2 = (x * 5039) >> 24
    x = x - t2 * q
    x = x - (q & ~((x - q) >> 31))
    return int(x)


def multiply_ntts(f: np.ndarray, g: np.ndarray) -> np.ndarray:
    h = np.zeros(N, dtype=np.int32)
    for i in range(N // 2):
        gamma = K_ALG11_GAMMAS[i]
        a0, a1 = int(f[2 * i]), int(f[2 * i + 1])
        b0, b1 = int(g[2 * i]), int(g[2 * i + 1])
        a1b1 = barrett_red_coeff(a1 * b1)
        h[2 * i] = barrett_red_coeff(a0 * b0 + a1b1 * gamma)
        h[2 * i + 1] = barrett_red_coeff(a0 * b1 + a1 * b0)
    return h


def a_hat_offset_jp(j: int, p: int) -> int:
    return (j * K + p) * N


def main() -> None:
    """生成 compute+tail golden：行 2/16–24（含 e₂+=μ 与 c.bin）。"""
    os.makedirs(os.path.join(_CASE_DIR, "input"), exist_ok=True)
    os.makedirs(os.path.join(_CASE_DIR, "output"), exist_ok=True)

    os.makedirs(os.path.join(_CASE_DIR, "golden"), exist_ok=True)

    # —— 随机输入 + μ ——
    rng = np.random.default_rng(SEED)
    m = rng.integers(0, 256, size=MSG_BYTES, dtype=np.uint8).tobytes()
    mu = mu_embed_from_m(m)
    y = rng.integers(0, Q, size=(K, N), dtype=np.int32)
    a_hat = rng.integers(0, Q, size=(K, K, N), dtype=np.int32)  # 存 A[p,j] 于 [p,j]
    t_hat = rng.integers(0, Q, size=(K, N), dtype=np.int32)
    e1 = rng.integers(-2, 3, size=(K, N), dtype=np.int32)
    e2 = rng.integers(-2, 3, size=(N,), dtype=np.int32)

    # —— 行 16–18：NTT + û + tr̂ ——
    y_hat = stage123_transform(y, "ntt")

    u_ntt = np.zeros((K, N), dtype=np.int32)
    for p in range(K):
        acc = np.zeros(N, dtype=np.int64)
        for j in range(K):
            ap = a_hat[j, p]
            prod = multiply_ntts(ap, y_hat[j])
            acc += prod.astype(np.int64)
        u_ntt[p] = acc % Q

    acc_tr = np.zeros(N, dtype=np.int64)
    for j in range(K):
        prod = multiply_ntts(t_hat[j], y_hat[j])
        acc_tr += prod.astype(np.int64)
    tr_hat_ntt = (acc_tr % Q).astype(np.int32)

    u_tr = np.zeros((K_P, N), dtype=np.int32)
    u_tr[:K] = u_ntt
    u_tr[4] = tr_hat_ntt

    u_tr_pad = np.zeros((K_INTT_PAD, N), dtype=np.int32)
    u_tr_pad[:K_P] = u_tr

    # —— 行 19/21：INTT；e₂+=μ 折叠后加噪；再 pack c ——
    time_pad = stage123_transform(u_tr_pad, "intt", K_INTT_PAD)
    u = ((time_pad[:K].astype(np.int64) + e1.astype(np.int64)) % Q).astype(np.int32)
    # 设备 Launch 1 前缀 e₂+=μ；v = INTT(tr̂) + e₂ + μ
    e2_eff = ((e2.astype(np.int64) + mu.astype(np.int64)) % Q).astype(np.int32)
    v = ((time_pad[4].astype(np.int64) + e2_eff.astype(np.int64)) % Q).astype(np.int32)
    c_golden = pack_ciphertext(u, v)

    # —— 行 2 ek + LUT + 落盘 ——
    ek_pke = np.zeros((K, 384), dtype=np.uint8)
    for j in range(K):
        for i in range(N // 2):
            t0 = int(t_hat[j, 2 * i]) & 0xFFF
            t1 = int(t_hat[j, 2 * i + 1]) & 0xFFF
            b0 = t0 & 0xFF
            b1 = ((t0 >> 8) & 0x0F) | ((t1 & 0x0F) << 4)
            b2 = (t1 >> 4) & 0xFF
            ek_pke[j, 3 * i + 0] = b0
            ek_pke[j, 3 * i + 1] = b1
            ek_pke[j, 3 * i + 2] = b2

    lut_ntt = load_lut_t_i8("ntt")
    lut_intt = load_lut_t_i8("intt")
    lut_ntt_even = lut_planar_stacked(lut_ntt, True)
    lut_ntt_odd = lut_planar_stacked(lut_ntt, False)
    lut_intt_even = lut_planar_stacked(lut_intt, True)
    lut_intt_odd = lut_planar_stacked(lut_intt, False)

    y.tofile(os.path.join(_CASE_DIR, "input", "y.bin"))
    with open(os.path.join(_CASE_DIR, "input", "m.bin"), "wb") as f:
        f.write(m)
    a_hat.reshape(K * K, N).tofile(os.path.join(_CASE_DIR, "input", "a_hat.bin"))
    ek_pke.reshape(-1).tofile(os.path.join(_CASE_DIR, "input", "ek_pke.bin"))
    e1.tofile(os.path.join(_CASE_DIR, "input", "e1.bin"))
    e2.tofile(os.path.join(_CASE_DIR, "input", "e2.bin"))
    lut_ntt_even.tofile(os.path.join(_CASE_DIR, "input", "lut_ntt_even_stacked.bin"))
    lut_ntt_odd.tofile(os.path.join(_CASE_DIR, "input", "lut_ntt_odd_stacked.bin"))
    lut_intt_even.tofile(os.path.join(_CASE_DIR, "input", "lut_intt_even_stacked.bin"))
    lut_intt_odd.tofile(os.path.join(_CASE_DIR, "input", "lut_intt_odd_stacked.bin"))

    y_hat.tofile(os.path.join(_CASE_DIR, "output", "golden_y_hat.bin"))
    u_ntt.tofile(os.path.join(_CASE_DIR, "output", "golden_u_ntt.bin"))
    u.tofile(os.path.join(_CASE_DIR, "output", "golden_u.bin"))
    v.tofile(os.path.join(_CASE_DIR, "output", "golden_v.bin"))
    u_tr.tofile(os.path.join(_CASE_DIR, "output", "golden_u_tr.bin"))
    tr_hat_ntt.tofile(os.path.join(_CASE_DIR, "output", "golden_tr_hat_ntt.bin"))
    with open(os.path.join(_CASE_DIR, "golden", "c.bin"), "wb") as f:
        f.write(c_golden)
    mu.tofile(os.path.join(_CASE_DIR, "golden", "mu_embed.bin"))

    # tiling 不再由 Python 落盘：运行时改由 f203_encrypt_tiling.cpp 的 GenerateTiling 生成
    # （模板风格，见该文件）；此处仅生成算法输入与 golden。

    print(f"[gen_data] k={K} seed={SEED} u/v/c golden c={C_BYTES}B (e2+=mu fold)")


if __name__ == "__main__":
    main()
