#!/usr/bin/env python3
# coding=utf-8
"""
gen_data.py — E10：prf→CBD→NTT→basemul→INTT→Compress_d→ByteEncode_d(d=4) golden + SHAKE 短向量。

流水线：run.sh 编译后、kernel 前。
写 input/* 与 output/golden.bin（=ByteEncode 128B）、golden_comp.bin、golden_cbd.bin、shake_golden.bin 等。

语义：CBD = FIPS Alg.8 η=2；NTT/INTT = ntt256 矩阵正/逆（≠ Tag5T）；
      basemul = FIPS Alg.11/12；Compress/ByteEncode = FIPS §4.2.1/Alg.5 d=4；
      SHAKE = hashlib.shake_256(b"abc").digest(32)。
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
from golden_se_sampling import sample_poly_cbd2  # noqa: E402

n = 256
Q = 3329
PRF_BYTES = 128
SEED = int(os.environ.get("SEED_D", "20260619"))
COMPRESS_D = int(os.environ.get("F203_COMPRESS_D", "4"))
ENCODE_D = int(os.environ.get("F203_BYTE_ENCODE_D", "4"))
ENCODE_BYTES = {4: 128, 5: 160, 10: 320, 11: 352}

_REF_COMP_C = os.path.join(ROOT, "vendor/compress_d/compress_d_ref.c")
_REF_COMP_H_DIR = os.path.join(ROOT, "vendor/compress_d")
_REF_COMP_SO = os.path.join(ROOT, "scripts", "libcompress_d_ref.so")

_REF_ENC_C = os.path.join(ROOT, "vendor/byteencode_d/byte_encode_d_ref.c")
_REF_ENC_H_DIR = os.path.join(ROOT, "vendor/byteencode_d")
_REF_ENC_SO = os.path.join(ROOT, "scripts", "libbyte_encode_d_ref.so")

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

_REF_FN_COMP = {
    4: "poly_compress_d4_ref",
    5: "poly_compress_d5_ref",
    10: "poly_compress_d10_ref",
    11: "poly_compress_d11_ref",
}

_REF_FN_ENC = {
    4: "poly_byte_encode_d4_ref",
    5: "poly_byte_encode_d5_ref",
    10: "poly_byte_encode_d10_ref",
    11: "poly_byte_encode_d11_ref",
}


def _build_ref_so(c_path: str, h_dir: str, so_path: str) -> None:
    cmd = ["gcc", "-shared", "-fPIC", "-O2", f"-I{h_dir}", "-o", so_path, c_path]
    subprocess.check_call(cmd)


def _compress_ref(poly: np.ndarray, d: int) -> np.ndarray:
    if d not in _REF_FN_COMP:
        raise ValueError(f"unsupported Compress d={d}")
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
    if d not in _REF_FN_ENC:
        raise ValueError(f"unsupported ByteEncode d={d}")
    if not os.path.isfile(_REF_ENC_SO):
        _build_ref_so(_REF_ENC_C, _REF_ENC_H_DIR, _REF_ENC_SO)
    lib = ctypes.CDLL(_REF_ENC_SO)
    fn = getattr(lib, _REF_FN_ENC[d])
    fn.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int32]
    fn.restype = None
    out = np.zeros(ENCODE_BYTES[d], dtype=np.uint8)
    comp = np.ascontiguousarray(comp, dtype=np.int32)
    fn(out.ctypes.data_as(ctypes.c_void_p),
       comp.ctypes.data_as(ctypes.c_void_p),
       ctypes.c_int32(comp.size))
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


def make_g_poly() -> np.ndarray:
    return np.array([(13 * i + 7) % Q for i in range(n)], dtype=np.int32)


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


def main() -> None:
    in_dir = os.path.join(ROOT, "input")
    out_dir = os.path.join(ROOT, "output")
    os.makedirs(in_dir, exist_ok=True)
    os.makedirs(out_dir, exist_ok=True)

    payload = struct.pack("<ii", 0, n)
    payload += b"\x00" * (64 - len(payload))
    with open(os.path.join(in_dir, "tiling.bin"), "wb") as f:
        f.write(payload)

    ntt_sim_kyber.gen_golden_data(n=n, q=3329, g=17)

    rng = np.random.default_rng(SEED)
    prf = rng.integers(0, 256, size=PRF_BYTES, dtype=np.uint8)
    prf.tofile(os.path.join(in_dir, "prf.bin"))
    cbd_poly = sample_poly_cbd2(bytes(prf)).astype(np.int32)
    cbd_poly.tofile(os.path.join(out_dir, "golden_cbd.bin"))
    cbd_poly.tofile(os.path.join(in_dir, "src.bin"))

    golden_ntt = ntt_sim_kyber.ntt_test01(n=n, q=3329, g=17, f=cbd_poly).astype(np.int32)
    golden_ntt.tofile(os.path.join(out_dir, "golden_ntt.bin"))

    g_hat = make_g_poly()
    g_hat.tofile(os.path.join(in_dir, "g.bin"))

    golden_h = alg11_multiply_ntts(golden_ntt, g_hat)
    golden_h.tofile(os.path.join(out_dir, "golden_basemul.bin"))

    m = ntt_sim_kyber.M.astype(np.int32)
    pack_m4_limbs(m).tofile(os.path.join(in_dir, "M4.bin"))

    Minv = mat_inv_mod(m, Q)
    rec = intt_via_minv(golden_ntt, Minv)
    assert np.array_equal(rec, cbd_poly % Q), "Minv failed NTT roundtrip on CBD poly"
    pack_m4_limbs(Minv.astype(np.int32)).tofile(os.path.join(in_dir, "Minv4.bin"))

    golden_intt = intt_via_minv(golden_h, Minv)
    golden_intt.tofile(os.path.join(out_dir, "golden_intt.bin"))

    golden_comp = _compress_ref(golden_intt, COMPRESS_D)
    golden_comp.tofile(os.path.join(out_dir, "golden_comp.bin"))

    golden_enc = _byteencode_ref(golden_comp, ENCODE_D)
    golden_enc.tofile(os.path.join(out_dir, "golden.bin"))
    golden_enc.tofile(os.path.join(out_dir, "golden_encode.bin"))

    shake_golden = hashlib.shake_256(b"abc").digest(32)
    with open(os.path.join(out_dir, "shake_golden.bin"), "wb") as f:
        f.write(shake_golden)

    print(
        f"[gen_data] E10 SHAKE+CBD+NTT+basemul+INTT+Compress_d={COMPRESS_D}+ByteEncode_d={ENCODE_D} "
        f"({ENCODE_BYTES[ENCODE_D]}B) under {in_dir} (SEED_D={SEED})"
    )


if __name__ == "__main__":
    main()
