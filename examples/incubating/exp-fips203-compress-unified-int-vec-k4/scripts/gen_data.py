#!/usr/bin/env python3
"""gen_data — 统一整数 Compress_d：随机 canonical poly + C ref golden（d=1/4/5/10/11）。"""
import os
import struct
import subprocess

import numpy as np
from ctypes import CDLL, c_int32, c_void_p

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_CASE_DIR = os.path.normpath(os.path.join(_SCRIPT_DIR, ".."))
_REF_C = os.path.join(_CASE_DIR, "compress_unified_int_ref.c")
_REF_SO = os.path.join(_CASE_DIR, "scripts", "libcompress_unified_int_ref.so")

N = 256
Q = 3329
SEED = 20260710
SUPPORTED_D = (1, 4, 5, 10, 11)


def _build_ref_so() -> None:
    os.makedirs(os.path.dirname(_REF_SO), exist_ok=True)
    d = int(os.environ.get("F203_UNIFIED_ROUND_D", "4"))
    cmd = [
        "gcc",
        "-shared",
        "-fPIC",
        "-O2",
        "-I",
        _CASE_DIR,
        f"-DF203_UNIFIED_ROUND_D={d}",
        "-o",
        _REF_SO,
        _REF_C,
    ]
    subprocess.run(cmd, check=True, cwd=_CASE_DIR)


def _compress_ref(poly: np.ndarray) -> np.ndarray:
    _build_ref_so()
    lib = CDLL(_REF_SO)
    out = np.zeros(N, dtype=np.int32)
    fn = lib.poly_compress_unified_ref
    fn.argtypes = [c_void_p, c_void_p, c_int32]
    fn.restype = None
    fn(out.ctypes.data_as(c_void_p), poly.ctypes.data_as(c_void_p), N)
    return out


def main() -> None:
    d = int(os.environ.get("F203_UNIFIED_ROUND_D", "4"))
    if d not in SUPPORTED_D:
        raise SystemExit(f"F203_UNIFIED_ROUND_D must be one of {SUPPORTED_D}")

    os.makedirs(os.path.join(_CASE_DIR, "input"), exist_ok=True)
    os.makedirs(os.path.join(_CASE_DIR, "output"), exist_ok=True)

    rng = np.random.default_rng(SEED + d)
    poly = rng.integers(0, Q, size=N, dtype=np.int32)
    golden = _compress_ref(poly)

    poly.tofile(os.path.join(_CASE_DIR, "input", "poly.bin"))
    golden.tofile(os.path.join(_CASE_DIR, "output", "golden_comp.bin"))

    meta = struct.pack("<ii", N, d)
    with open(os.path.join(_CASE_DIR, "input", "meta.bin"), "wb") as f:
        f.write(meta)

    print(f"[gen_data] N={N} d={d} poly range [0,{Q}) golden max={golden.max()}")


if __name__ == "__main__":
    main()
