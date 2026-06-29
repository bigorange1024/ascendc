#!/usr/bin/env python3
# coding=utf-8
"""
Golden generator for pass-fix-f203-alg11-12-multiplyntts-k4.

FIPS 203 Alg.11/12:
  f, g — length-256 polynomials in Z_q (canonical coeffs in [0,q))
  gamma[i] = kMlkemGammas[i] per BaseCaseMultiply pair
"""
import ctypes
import os
import subprocess
import sys

import numpy as np

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_CASE_DIR = os.path.normpath(os.path.join(_SCRIPT_DIR, ".."))

N = 256
Q = 3329

# sync alg11_gammas.h
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


def make_f_poly() -> np.ndarray:
    """Toy f̂ ∈ R_q: varied canonical coeffs (not constant)."""
    return np.array([(17 * i + 3) % Q for i in range(N)], dtype=np.int32)


def make_g_poly() -> np.ndarray:
    """Toy ĝ ∈ R_q: different pattern from f."""
    return np.array([(13 * i + 7) % Q for i in range(N)], dtype=np.int32)


def barrett_red_coeff(x: int) -> int:
    q = Q
    t = x + (q & (x >> 31))
    t1 = (t * 78) >> 18
    x = t - t1 * q
    t2 = (x * 5039) >> 24
    x = x - t2 * q
    x = x - (q & ~((x - q) >> 31))
    return int(x)


def alg12_base_case_multiply(a0: int, a1: int, b0: int, b1: int, gamma: int) -> tuple[int, int]:
    a1b1 = barrett_red_coeff(a1 * b1)
    c0 = barrett_red_coeff(a0 * b0 + a1b1 * gamma)
    c1 = barrett_red_coeff(a0 * b1 + a1 * b0)
    return c0, c1


def alg11_multiply_ntts_py(f: np.ndarray, g: np.ndarray) -> np.ndarray:
    h = np.zeros(N, dtype=np.int32)
    for i in range(N // 2):
        a0, a1 = int(f[i * 2]), int(f[i * 2 + 1])
        b0, b1 = int(g[i * 2]), int(g[i * 2 + 1])
        c0, c1 = alg12_base_case_multiply(a0, a1, b0, b1, K_ALG11_GAMMAS[i])
        h[i * 2] = c0
        h[i * 2 + 1] = c1
    return h


def build_c_ref():
    src = os.path.join(_CASE_DIR, "alg11_12_ref.c")
    hdr_dir = _CASE_DIR
    out = os.path.join(_SCRIPT_DIR, "_alg11_ref.so")
    cmd = [
        "gcc",
        "-shared",
        "-fPIC",
        "-O2",
        "-o",
        out,
        src,
        f"-I{hdr_dir}",
    ]
    subprocess.check_call(cmd)
    lib = ctypes.CDLL(out)
    fn = lib.alg11_multiply_ntts
    fn.argtypes = [
        ctypes.POINTER(ctypes.c_int32),
        ctypes.POINTER(ctypes.c_int32),
        ctypes.POINTER(ctypes.c_int32),
    ]
    fn.restype = None
    return fn, out


def alg11_multiply_ntts_c(fn, f: np.ndarray, g: np.ndarray) -> np.ndarray:
    h = np.zeros(N, dtype=np.int32)
    fn(
        h.ctypes.data_as(ctypes.POINTER(ctypes.c_int32)),
        f.ctypes.data_as(ctypes.POINTER(ctypes.c_int32)),
        g.ctypes.data_as(ctypes.POINTER(ctypes.c_int32)),
    )
    return h


def main() -> None:
    os.makedirs(os.path.join(_CASE_DIR, "input"), exist_ok=True)
    os.makedirs(os.path.join(_CASE_DIR, "output"), exist_ok=True)

    f = make_f_poly()
    g = make_g_poly()
    golden_py = alg11_multiply_ntts_py(f, g)

    fn, so_path = build_c_ref()
    golden_c = alg11_multiply_ntts_c(fn, f, g)
    if not np.array_equal(golden_py, golden_c):
        diff = np.where(golden_py != golden_c)[0][:8]
        print("[ERROR] Python vs C ref mismatch at indices:", diff)
        sys.exit(1)
    os.remove(so_path)

    f.tofile(os.path.join(_CASE_DIR, "input", "a.bin"))
    g.tofile(os.path.join(_CASE_DIR, "input", "b.bin"))
    golden_py.tofile(os.path.join(_CASE_DIR, "output", "golden_h.bin"))

    print(f"N={N} q={Q} gamma=kMlkemGammas[128]")
    print(f"f[0:6] = {f[:6].tolist()}  g[0:6] = {g[:6].tolist()}")
    print(f"h[0:6] = {golden_py[:6].tolist()}")
    print(f"gamma[0:4] = {K_ALG11_GAMMAS[:4]}")
    print("[OK] gen_data: input/a.bin input/b.bin output/golden_h.bin")


if __name__ == "__main__":
    main()
