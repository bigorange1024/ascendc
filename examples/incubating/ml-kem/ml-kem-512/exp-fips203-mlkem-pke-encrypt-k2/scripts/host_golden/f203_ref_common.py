#!/usr/bin/env python3
"""
@file f203_ref_common.py
@brief host_golden 共用 FIPS 203 参考实现（禁止 liboqs）。

流水线：被 gen_ek_pke / golden_c / gen_data 引用；提供 NTT/INTT Stage123、
Compress/ByteEncode、Alg.11 basemul、消息嵌入等。仅作 golden I/O oracle，非设备规格。
"""
from __future__ import annotations

import hashlib
import re
from pathlib import Path

import numpy as np

K = 2
N = 256
Q = 3329
HALF_N = N // 2
LIMBS = 4
MAT_C_PLANAR_ROWS = K * LIMBS * 2
LIMB_BITS = 6
XOF_BYTES = 672
CAND_PAIRS = XOF_BYTES // 3

CASE = Path(__file__).resolve().parent.parent.parent


def _repo_root() -> Path:
    """向上定位仓库根，避免 ml-kem/ml-kem-*/ 多层目录导致 LUT 相对路径失效。"""
    cur = CASE
    while cur != cur.parent:
        if (cur / "AGENTS.md").is_file() and (cur / "thirdparty").is_dir():
            return cur
        cur = cur.parent
    raise RuntimeError(f"cannot locate repo root from {CASE}")


# 集成探针：LUT 头取仓库级 thirdparty/ntt_onnx（本探针不 vendored compute/ntt_r 子树）。
LUT_HDR = _repo_root() / "thirdparty/ntt_onnx/include/mlkem/stable/transpose_mlkem_luts_i8.h"

GAMMAS = np.array(
    [
        17, 3312, 2761, 568, 583, 2746, 2649, 680, 1637, 1692, 723, 2606, 2288, 1041, 1100, 2229, 1409, 1920,
        2662, 667, 3281, 48, 233, 3096, 756, 2573, 2156, 1173, 3015, 314, 3050, 279, 1703, 1626, 1651, 1678,
        2789, 540, 1789, 1540, 1847, 1482, 952, 2377, 1461, 1868, 2687, 642, 939, 2390, 2308, 1021, 2437, 892,
        2388, 941, 733, 2596, 2337, 992, 268, 3061, 641, 2688, 1584, 1745, 2298, 1031, 2037, 1292, 3220, 109,
        375, 2954, 2549, 780, 2090, 1239, 1645, 1684, 1063, 2266, 319, 3010, 2773, 556, 757, 2572, 2099, 1230,
        561, 2768, 2466, 863, 2594, 735, 2804, 525, 1092, 2237, 403, 2926, 1026, 2303, 1143, 2186, 2150, 1179,
        2775, 554, 886, 2443, 1722, 1607, 1212, 2117, 1874, 1455, 1029, 2300, 2110, 1219, 2935, 394, 885, 2444,
        2154, 1175,
    ],
    dtype=np.int32,
)


def mod_q_i64(x: int) -> int:
    """整数 mod q，结果落在 [0,q)。"""
    rem = x % Q
    if rem < 0:
        rem += Q
    return int(rem)


def barrett_red(x: int) -> int:
    """Barrett 约化到 [0,q)（Alg.11 basemul 用）。"""
    t = x + (Q & (x >> 31))
    t1 = (t * 78) >> 18
    x = t - t1 * Q
    t2 = (x * 5039) >> 24
    x = x - t2 * Q
    x = x - (Q & ~((x - Q) >> 31))
    return int(x)


def multiply_ntts(f: np.ndarray, g: np.ndarray) -> np.ndarray:
    """Alg.11：NTT 域逐对 basemul（含 γ）。输入/输出 int32[N]。"""
    h = np.zeros(N, dtype=np.int32)
    for i in range(N // 2):
        gamma = int(GAMMAS[i])
        a0, a1 = int(f[2 * i]), int(f[2 * i + 1])
        b0, b1 = int(g[2 * i]), int(g[2 * i + 1])
        a1b1 = barrett_red(a1 * b1)
        h[2 * i] = barrett_red(a0 * b0 + a1b1 * gamma)
        h[2 * i + 1] = barrett_red(a0 * b1 + a1 * b0)
    return h


def load_lut_t_i8(mode: str) -> np.ndarray:
    """从 thirdparty LUT 头解析 T_i8[N,512]（ntt/intt）。仅 golden 读表。"""
    symbol = "kMlkemLimb6Ntt_T_i8" if mode == "ntt" else "kMlkemLimb6Intt_T_i8"
    txt = LUT_HDR.read_text(encoding="utf-8")
    i0 = txt.index(symbol)
    i1 = txt.index("{", i0)
    i2 = txt.index("};", i1)
    nums = [int(x) for x in re.findall(r"-?\d+", txt[i1 + 1 : i2])]
    return np.array(nums, dtype=np.int8).reshape(N, 512)


def a_hat_offset(p: int, j: int) -> int:
    """KeyGen 风格扁平偏移：(p*K+j)*N。"""
    return (p * K + j) * N


def a_hat_offset_at(p: int, j: int) -> int:
    """Encrypt handoff 偏移：(j*K+p)*N（与 prep 存储一致）。"""
    return (j * K + p) * N


def planar_row(slot: int, limb: int, half: int) -> int:
    """平面 mat_c 行号：half×(K*LIMBS)+slot*LIMBS+limb。"""
    return half * (K * LIMBS) + slot * LIMBS + limb


def encode_compact(polys: np.ndarray) -> np.ndarray:
    """Stage1：polyvec → s0[2K,N] int8（hi/lo 各 6-bit）。"""
    s0 = np.zeros((2 * K, N), dtype=np.int8)
    for lp in range(K):
        for r in range(N):
            v = int(polys[lp, r]) % Q
            s0[lp, r] = (v >> LIMB_BITS) & 0x3F
            s0[K + lp, r] = v & 0x3F
    return s0


def mat_c_tmp_golden(s0: np.ndarray, lut: np.ndarray) -> tuple[np.ndarray, ...]:
    """Stage2 参考：s0 @ LUT 四路 → (lo_even, lo_odd, hi_even, hi_odd)。"""
    le = lut[:, 0:N:2]
    lo = lut[:, 1:N:2]
    he = lut[:, N:512:2]
    ho = lut[:, N + 1 : 512 : 2]
    c_lo_even = (s0.astype(np.int32) @ le.astype(np.int32)).astype(np.int32)
    c_lo_odd = (s0.astype(np.int32) @ lo.astype(np.int32)).astype(np.int32)
    c_hi_even = (s0.astype(np.int32) @ he.astype(np.int32)).astype(np.int32)
    c_hi_odd = (s0.astype(np.int32) @ ho.astype(np.int32)).astype(np.int32)
    return c_lo_even, c_lo_odd, c_hi_even, c_hi_odd


def pack_bank(c_le, c_lo, c_he, c_ho, poly_base: int, k_polys: int, out: np.ndarray) -> None:
    """将四路临时矩阵写入平面 mat_c 的一组 poly slot。"""
    for lp in range(k_polys):
        hi_r = poly_base + lp
        lo_r = K + poly_base + lp
        slot = poly_base + lp
        out[planar_row(slot, 0, 0), :] = c_le[hi_r, :]
        out[planar_row(slot, 1, 0), :] = c_lo[hi_r, :]
        out[planar_row(slot, 2, 0), :] = c_le[lo_r, :]
        out[planar_row(slot, 3, 0), :] = c_lo[lo_r, :]
        out[planar_row(slot, 0, 1), :] = c_he[hi_r, :]
        out[planar_row(slot, 1, 1), :] = c_ho[hi_r, :]
        out[planar_row(slot, 2, 1), :] = c_he[lo_r, :]
        out[planar_row(slot, 3, 1), :] = c_ho[lo_r, :]


def pack_mat_c_planar(c_le, c_lo, c_he, c_ho) -> np.ndarray:
    """打包完整平面 mat_c[MAT_C_PLANAR_ROWS, HALF_N]。"""
    out = np.zeros((MAT_C_PLANAR_ROWS, HALF_N), dtype=np.int32)
    pack_bank(c_le, c_lo, c_he, c_ho, 0, K, out)
    return out


def stage31_mod(raw: np.ndarray) -> np.ndarray:
    """Stage3 参考 mod q（向零截断商后校正到 [0,q)）。"""
    raw64 = raw.astype(np.int64)
    q = np.int64(Q)
    t = np.where(raw64 >= 0, raw64 // q, -((-raw64) // q))
    rem = raw64 - q * t
    rem = rem - q * (rem >= q).astype(np.int64)
    rem = rem + q * (rem < 0).astype(np.int64)
    return rem.astype(np.int32)


def merge_planar_poly(mat_planar: np.ndarray, slot: int) -> np.ndarray:
    """RouteA：四 limb 行 Horner 合并为 int32[N]（lo/hi 半区）。"""
    hh = mat_planar[planar_row(slot, 0, 0)].astype(np.int64)
    lh = mat_planar[planar_row(slot, 1, 0)].astype(np.int64)
    hl = mat_planar[planar_row(slot, 2, 0)].astype(np.int64)
    ll = mat_planar[planar_row(slot, 3, 0)].astype(np.int64)
    raw_lo = hh * 4096 + (hl + lh) * 64 + ll
    hh = mat_planar[planar_row(slot, 0, 1)].astype(np.int64)
    lh = mat_planar[planar_row(slot, 1, 1)].astype(np.int64)
    hl = mat_planar[planar_row(slot, 2, 1)].astype(np.int64)
    ll = mat_planar[planar_row(slot, 3, 1)].astype(np.int64)
    raw_hi = hh * 4096 + (hl + lh) * 64 + ll
    out = np.zeros(N, dtype=np.int32)
    out[:HALF_N] = stage31_mod(raw_lo.astype(np.int32))
    out[HALF_N:] = stage31_mod(raw_hi.astype(np.int32))
    return out


def stage123_transform(polys: np.ndarray, mode: str) -> np.ndarray:
    """k=2 polyvec Stage123 NTT/INTT；device INTT 复用 polyvec4，但 golden 按语义 poly 独立变换。"""
    if polys.shape != (K, N):
        raise ValueError(f"expected ({K},{N}) got {polys.shape}")
    lut = load_lut_t_i8(mode)
    s0 = encode_compact(polys)
    c_le, c_lo, c_he, c_ho = mat_c_tmp_golden(s0, lut)
    mat_planar = pack_mat_c_planar(c_le, c_lo, c_he, c_ho)
    dst = np.zeros((K, N), dtype=np.int32)
    for slot in range(K):
        dst[slot] = merge_planar_poly(mat_planar, slot)
    return dst


def compress_d_scalar(u: int, d: int) -> int:
    """Compress_d 标量（ML-KEM-512 密文域默认 d=10/4；保留通用整数公式）。"""
    u = int(u) % Q
    if d in (4, 10):
        # 等价 round((2^d / q) * u) mod 2^d；仅 golden I/O oracle。
        return int((((u << d) + Q // 2) // Q) & ((1 << d) - 1))
    raise ValueError(f"unsupported d={d}")


def byte_encode_d(F: np.ndarray, d: int) -> bytes:
    """FIPS 203 Alg.5：256 个 d-bit 整数 → 32*d 字节。"""
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


def poly_byte_encode12(poly: np.ndarray) -> bytes:
    """Alg.5 d=12：int32[N] → 384B。"""
    out = bytearray(384)
    for i in range(128):
        t0 = int(poly[2 * i]) % Q
        t1 = int(poly[2 * i + 1]) % Q
        out[3 * i] = t0 & 0xFF
        out[3 * i + 1] = ((t0 >> 8) & 0x0F) | ((t1 & 0x0F) << 4)
        out[3 * i + 2] = (t1 >> 4) & 0xFF
    return bytes(out)


def pack_ciphertext(u: np.ndarray, v: np.ndarray) -> bytes:
    """ML-KEM-512：c₁=2×ByteEncode₁₀(Compress₁₀(u))，c₂=ByteEncode₄(Compress₄(v))。"""
    c1 = bytearray(K * 320)
    for p in range(K):
        comp = np.array([compress_d_scalar(int(x), 10) for x in u[p]], dtype=np.int32)
        c1[p * 320 : (p + 1) * 320] = byte_encode_d(comp, 10)
    comp_v = np.array([compress_d_scalar(int(x), 4) for x in v], dtype=np.int32)
    c2 = byte_encode_d(comp_v, 4)
    return bytes(c1) + c2


def embed_message(v: np.ndarray, m: bytes) -> np.ndarray:
    """行 20–21：v ← v + HALF_Q·bit(m)（mod q）。"""
    out = v.copy()
    half_q = (Q + 1) // 2
    for i in range(32):
        for j in range(8):
            idx = 8 * i + j
            bit = (m[i] >> j) & 1
            out[idx] = mod_q_i64(int(out[idx]) + half_q * bit)
    return out
