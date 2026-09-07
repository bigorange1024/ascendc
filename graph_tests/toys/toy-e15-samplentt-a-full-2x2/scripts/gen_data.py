#!/usr/bin/env python3
# coding=utf-8
"""
gen_data.py — E15：E14 壳 + 完整 2×2 Â 真 SampleNTT golden。

u 路 c1：SampleNTT(SEED_D,(0,0)/(0,1))；v 路 c2：SampleNTT(SEED_D,(1,0))；G3=(1,1) 完整性。
"""
import ctypes
import hashlib
import os
import struct
import subprocess
import sys
import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPO = os.path.abspath(os.path.join(ROOT, "../../.."))
sys.path.insert(0, os.path.join(ROOT, "scripts"))
sys.path.insert(0, os.path.join(REPO, "library/shared/fips203_se_sample"))
import ntt_sim_kyber  # noqa: E402
from golden_se_sampling import sample_poly_cbd2, derand_bytes_from_seed  # noqa: E402
from alg7_geom import CAND_PAIRS, XOF_BYTES  # noqa: E402

k = 2
n = 256
Q = 3329
PRF_BYTES_PER_POLY = 128
PRF_POLYS = k + 1
PRF_BYTES = PRF_BYTES_PER_POLY * PRF_POLYS
MU_BYTES = 32
SEED = int(os.environ.get("SEED_D", "20260619"))
COMPRESS_D = int(os.environ.get("F203_COMPRESS_D", "4"))
ENCODE_D = int(os.environ.get("F203_BYTE_ENCODE_D", "4"))
ENCODE_BYTES_PER_POLY = {4: 128, 5: 160, 10: 320, 11: 352}
C1_BYTES = ENCODE_BYTES_PER_POLY[ENCODE_D] * k
C2_BYTES = ENCODE_BYTES_PER_POLY[ENCODE_D]
ENCODE_OUT = C1_BYTES + C2_BYTES

_REF_COMP_C = os.path.join(ROOT, "vendor/compress_d/compress_d_ref.c")
_REF_COMP_H_DIR = os.path.join(ROOT, "vendor/compress_d")
_REF_COMP_SO = os.path.join(ROOT, "scripts", "libcompress_d_ref.so")

_REF_ENC_C = os.path.join(ROOT, "vendor/byteencode_d/byte_encode_d_ref.c")
_REF_ENC_H_DIR = os.path.join(ROOT, "vendor/byteencode_d")
_REF_ENC_SO = os.path.join(ROOT, "scripts", "libbyte_encode_d_ref.so")

_REF_DEC1_C = os.path.join(ROOT, "vendor/decompress_d/decompress_d1_ref.c")
_REF_DEC1_H_DIR = os.path.join(ROOT, "vendor/decompress_d")
_REF_DEC1_SO = os.path.join(ROOT, "scripts", "libdecompress_d1_ref.so")

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

_REF_FN_COMP = {4: "poly_compress_d4_ref", 5: "poly_compress_d5_ref", 10: "poly_compress_d10_ref", 11: "poly_compress_d11_ref"}
_REF_FN_ENC = {4: "poly_byte_encode_d4_ref", 5: "poly_byte_encode_d5_ref", 10: "poly_byte_encode_d10_ref", 11: "poly_byte_encode_d11_ref"}


def _build_ref_so(c_path: str, h_dir: str, so_path: str) -> None:
    cmd = ["gcc", "-shared", "-fPIC", "-O2", f"-I{h_dir}", "-o", so_path, c_path]
    subprocess.check_call(cmd)


def _compress_ref(poly: np.ndarray, d: int) -> np.ndarray:
    if not os.path.isfile(_REF_COMP_SO):
        _build_ref_so(_REF_COMP_C, _REF_COMP_H_DIR, _REF_COMP_SO)
    lib = ctypes.CDLL(_REF_COMP_SO)
    fn = getattr(lib, _REF_FN_COMP[d])
    fn.argtypes = [ctypes.POINTER(ctypes.c_int32), ctypes.POINTER(ctypes.c_int32), ctypes.c_int32]
    fn.restype = None
    inn = np.ascontiguousarray(poly, dtype=np.int32)
    out = np.zeros_like(inn)
    fn(out.ctypes.data_as(ctypes.POINTER(ctypes.c_int32)),
       inn.ctypes.data_as(ctypes.POINTER(ctypes.c_int32)),
       ctypes.c_int32(inn.size))
    return out


def _byteencode_ref(comp: np.ndarray, d: int) -> np.ndarray:
    if not os.path.isfile(_REF_ENC_SO):
        _build_ref_so(_REF_ENC_C, _REF_ENC_H_DIR, _REF_ENC_SO)
    lib = ctypes.CDLL(_REF_ENC_SO)
    fn = getattr(lib, _REF_FN_ENC[d])
    fn.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int32]
    fn.restype = None
    out = np.zeros(ENCODE_BYTES_PER_POLY[d], dtype=np.uint8)
    comp = np.ascontiguousarray(comp, dtype=np.int32)
    fn(out.ctypes.data_as(ctypes.c_void_p),
       comp.ctypes.data_as(ctypes.c_void_p),
       ctypes.c_int32(comp.size))
    return out


def _embed_message_ref(poly: np.ndarray, mu: np.ndarray) -> np.ndarray:
    if not os.path.isfile(_REF_DEC1_SO):
        _build_ref_so(_REF_DEC1_C, _REF_DEC1_H_DIR, _REF_DEC1_SO)
    lib = ctypes.CDLL(_REF_DEC1_SO)
    fn = lib.embed_message_ref
    fn.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int32]
    fn.restype = None
    out = np.zeros(n, dtype=np.int32)
    inn = np.ascontiguousarray(poly, dtype=np.int32)
    m = np.ascontiguousarray(mu, dtype=np.uint8)
    fn(out.ctypes.data_as(ctypes.c_void_p),
       inn.ctypes.data_as(ctypes.c_void_p),
       m.ctypes.data_as(ctypes.c_void_p),
       ctypes.c_int32(n))
    return out


def barrett_red_coeff(x: int) -> int:
    q = Q
    t = x + (q & (x >> 31))
    t1 = (t * 78) >> 18
    x = t - t1 * q
    t2 = (x * 5039) >> 24
    x = x - t2 * q
    x = x - (q & ~((x - q) >> 31))
    return int(x)


def alg12_base_case_multiply(a0: int, a1: int, b0: int, b1: int, gamma: int):
    a1b1 = barrett_red_coeff(a1 * b1)
    c0 = barrett_red_coeff(a0 * b0 + a1b1 * gamma)
    c1 = barrett_red_coeff(a0 * b1 + a1 * b0)
    return c0, c1


def alg11_multiply_ntts(f: np.ndarray, g: np.ndarray) -> np.ndarray:
    h = np.zeros(n, dtype=np.int32)
    for i in range(n // 2):
        c0, c1 = alg12_base_case_multiply(
            int(f[i * 2]), int(f[i * 2 + 1]), int(g[i * 2]), int(g[i * 2 + 1]), K_ALG11_GAMMAS[i]
        )
        h[i * 2] = c0
        h[i * 2 + 1] = c1
    return h


def hash_g_rho(d: bytes, kyber_k: int = 2) -> bytes:
    return hashlib.sha3_512(d + bytes([kyber_k & 0xFF])).digest()[:32]


def shake128_squeeze(msg: bytes, outlen: int) -> bytes:
    return hashlib.shake_128(msg).digest(outlen)


def unpack_d12_from_xof(buf: bytes) -> tuple[np.ndarray, np.ndarray]:
    d1 = np.empty(CAND_PAIRS, dtype=np.int32)
    d2 = np.empty(CAND_PAIRS, dtype=np.int32)
    pos = 0
    for t in range(CAND_PAIRS):
        c0, c1, c2 = buf[pos], buf[pos + 1], buf[pos + 2]
        d1[t] = c0 + 256 * (c1 & 0x0F)
        d2[t] = (c1 >> 4) + 16 * c2
        pos += 3
    return d1, d2


def rej_scalar_from_d12(d1: np.ndarray, d2: np.ndarray, q: int = Q, nn: int = n) -> np.ndarray:
    out: list[int] = []
    for i in range(d1.shape[0]):
        v1 = int(d1[i])
        if v1 < q and len(out) < nn:
            out.append(v1)
        v2 = int(d2[i])
        if v2 < q and len(out) < nn:
            out.append(v2)
    if len(out) < nn:
        raise RuntimeError(f"rej: only {len(out)} coeffs from {XOF_BYTES}B xof")
    return np.array(out[:nn], dtype=np.int32)


def sample_ntt_a_hat(seed_d: int, poly_j: int, poly_i: int) -> np.ndarray:
    """Alg.7 SampleNTT golden：SEED_D + (j,i) → â[256] int32。"""
    d = derand_bytes_from_seed(seed_d, kyber_k=2)
    rho = hash_g_rho(d)
    seed = rho + bytes([poly_j & 0xFF, poly_i & 0xFF])
    xof = shake128_squeeze(seed, XOF_BYTES)
    d1, d2 = unpack_d12_from_xof(xof)
    return rej_scalar_from_d12(d1, d2)


def make_g_matrix_full_2x2(seed_d: int) -> tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
    """完整 k×k=2×2 SampleNTT oracle。"""
    g00 = sample_ntt_a_hat(seed_d, 0, 0)
    g01 = sample_ntt_a_hat(seed_d, 0, 1)
    g10 = sample_ntt_a_hat(seed_d, 1, 0)
    g11 = sample_ntt_a_hat(seed_d, 1, 1)
    return g00, g01, g10, g11


def modinv(a: int, p: int) -> int:
    return pow(int(a) % p, p - 2, p)


def mat_inv_mod(A: np.ndarray, p: int) -> np.ndarray:
    A = A.astype(object).copy()
    nn = A.shape[0]
    I = np.eye(nn, dtype=object)
    Aug = np.concatenate([A, I], axis=1)
    for col in range(nn):
        piv = None
        for r in range(col, nn):
            if Aug[r, col] % p != 0:
                piv = r
                break
        if piv is None:
            raise ValueError("singular matrix mod q")
        if piv != col:
            Aug[[col, piv]] = Aug[[piv, col]]
        inv = modinv(Aug[col, col], p)
        Aug[col] = [(v * inv) % p for v in Aug[col]]
        for r in range(nn):
            if r == col:
                continue
            f = Aug[r, col] % p
            if f:
                Aug[r] = [(Aug[r, c] - f * Aug[col, c]) % p for c in range(2 * nn)]
    return Aug[:, nn:].astype(np.int64)


def pack_m4_limbs(m: np.ndarray) -> np.ndarray:
    m = m.astype(np.int32)
    m0 = ((m >> 0) & 0x7F).astype(np.int8).reshape(-1)
    m1 = ((m >> 7) & 0x7F).astype(np.int8).reshape(-1)
    m2 = ((m >> 14) & 0x7F).astype(np.int8).reshape(-1)
    m3 = ((m >> 21) & 0x7F).astype(np.int8).reshape(-1)
    return np.concatenate((m0, m1, m2, m3)).astype(np.int8)


def intt_via_minv(h: np.ndarray, Minv: np.ndarray) -> np.ndarray:
    return (h.astype(np.int64) @ Minv.astype(np.int64) % Q).astype(np.int32)


def chain_u_poly(cbd_poly: np.ndarray, g_hat: np.ndarray, Minv: np.ndarray) -> np.ndarray:
    golden_ntt = ntt_sim_kyber.ntt_test01(n=n, q=3329, g=17, f=cbd_poly).astype(np.int32)
    golden_h = alg11_multiply_ntts(golden_ntt, g_hat)
    golden_intt = intt_via_minv(golden_h, Minv)
    golden_comp = _compress_ref(golden_intt, COMPRESS_D)
    return _byteencode_ref(golden_comp, ENCODE_D)


def chain_v_poly(e2_poly: np.ndarray, g_v: np.ndarray, mu: np.ndarray, Minv: np.ndarray) -> np.ndarray:
    golden_ntt = ntt_sim_kyber.ntt_test01(n=n, q=3329, g=17, f=e2_poly).astype(np.int32)
    golden_h = alg11_multiply_ntts(golden_ntt, g_v)
    golden_intt = intt_via_minv(golden_h, Minv)
    golden_intt_plus_mu = _embed_message_ref(golden_intt, mu)
    golden_comp = _compress_ref(golden_intt_plus_mu, COMPRESS_D)
    return _byteencode_ref(golden_comp, ENCODE_D)


def main() -> None:
    in_dir = os.path.join(ROOT, "input")
    out_dir = os.path.join(ROOT, "output")
    os.makedirs(in_dir, exist_ok=True)
    os.makedirs(out_dir, exist_ok=True)

    payload = struct.pack("<ii", 0, n)
    payload += b"\x00" * (64 - len(payload))
    with open(os.path.join(in_dir, "tiling.bin"), "wb") as f:
        f.write(payload)

    with open(os.path.join(in_dir, "seed_d.bin"), "wb") as f:
        f.write(struct.pack("<I", SEED))

    ntt_sim_kyber.gen_golden_data(n=n, q=3329, g=17)

    rng = np.random.default_rng(SEED)
    prf = rng.integers(0, 256, size=PRF_BYTES, dtype=np.uint8)
    prf.tofile(os.path.join(in_dir, "prf.bin"))

    u_polys = []
    for p in range(k):
        row_prf = prf[p * PRF_BYTES_PER_POLY : (p + 1) * PRF_BYTES_PER_POLY]
        u_polys.append(sample_poly_cbd2(bytes(row_prf)).astype(np.int32))
    u_all = np.concatenate(u_polys)
    u_all.tofile(os.path.join(out_dir, "golden_cbd_u.bin"))
    u_all.tofile(os.path.join(in_dir, "src.bin"))

    v_prf = prf[k * PRF_BYTES_PER_POLY : (k + 1) * PRF_BYTES_PER_POLY]
    e2_poly = sample_poly_cbd2(bytes(v_prf)).astype(np.int32)
    e2_poly.tofile(os.path.join(out_dir, "golden_cbd_v.bin"))

    mu_rng = np.random.default_rng(SEED + 1)
    mu = mu_rng.integers(0, 256, size=MU_BYTES, dtype=np.uint8)
    mu.tofile(os.path.join(in_dir, "mu.bin"))

    # 完整 2×2 Â：四元真 SampleNTT；v 路 basemul 用 (1,0)
    g00, g01, g10, g11 = make_g_matrix_full_2x2(SEED)
    g00.tofile(os.path.join(out_dir, "golden_a_hat_00.bin"))
    g01.tofile(os.path.join(out_dir, "golden_a_hat_01.bin"))
    g10.tofile(os.path.join(out_dir, "golden_a_hat_10.bin"))
    g11.tofile(os.path.join(out_dir, "golden_a_hat_11.bin"))
    g_all = np.concatenate([g00, g01, g10, g11])
    g_all.tofile(os.path.join(out_dir, "golden_g_full2x2.bin"))

    m = ntt_sim_kyber.M.astype(np.int32)
    pack_m4_limbs(m).tofile(os.path.join(in_dir, "M4.bin"))
    Minv = mat_inv_mod(m, Q)
    pack_m4_limbs(Minv.astype(np.int32)).tofile(os.path.join(in_dir, "Minv4.bin"))

    c1_parts = []
    for p, g_hat in enumerate([g00, g01]):
        enc = chain_u_poly(u_polys[p], g_hat, Minv)
        c1_parts.append(enc)
        enc.tofile(os.path.join(out_dir, f"golden_c1_poly{p}.bin"))
    c1 = np.concatenate(c1_parts)

    c2 = chain_v_poly(e2_poly, g10, mu, Minv)
    c2.tofile(os.path.join(out_dir, "golden_c2.bin"))

    golden_c = np.concatenate([c1, c2])
    golden_c.tofile(os.path.join(out_dir, "golden.bin"))
    golden_c.tofile(os.path.join(out_dir, "golden_c.bin"))
    c1.tofile(os.path.join(out_dir, "golden_c1.bin"))

    u_all.tofile(os.path.join(out_dir, "golden_cbd.bin"))

    shake_golden = hashlib.shake_256(b"abc").digest(32)
    with open(os.path.join(out_dir, "shake_golden.bin"), "wb") as f:
        f.write(shake_golden)

    print(
        f"[gen_data] E15 full 2×2 SampleNTT: c1={C1_BYTES}B c2={C2_BYTES}B total={ENCODE_OUT}B "
        f"Â=(0,0)(0,1)(1,0)(1,1) v=G2=(1,0) SEED_D={SEED}"
    )


if __name__ == "__main__":
    main()
