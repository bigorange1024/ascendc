#!/usr/bin/env python3
"""gen_data — Decompress_d 探针：随机 d-bit comp + C ref golden。"""
import os
import struct
import subprocess
import sys

import numpy as np
from ctypes import CDLL, c_int32, c_void_p

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_CASE_DIR = os.path.normpath(os.path.join(_SCRIPT_DIR, ".."))
_REF_C = os.path.join(_CASE_DIR, "decompress_d_ref.c")
_REF_SO = os.path.join(_CASE_DIR, "scripts", "libdecompress_d_ref.so")

N = 256
SEED = 20260628


def _build_ref_so() -> None:
    os.makedirs(os.path.dirname(_REF_SO), exist_ok=True)
    cmd = [
        "gcc",
        "-shared",
        "-fPIC",
        "-O2",
        "-I",
        _CASE_DIR,
        "-o",
        _REF_SO,
        _REF_C,
    ]
    subprocess.run(cmd, check=True, cwd=_CASE_DIR)


def _decompress_ref(comp: np.ndarray, d: int) -> np.ndarray:
    _build_ref_so()
    lib = CDLL(_REF_SO)
    out = np.zeros(N, dtype=np.int32)
    if d == 4:
        fn = lib.poly_decompress_d4_ref
    elif d == 10:
        fn = lib.poly_decompress_d10_ref
    else:
        raise SystemExit(f"unsupported d={d}")
    fn.argtypes = [c_void_p, c_void_p, c_int32]
    fn.restype = None
    fn(out.ctypes.data_as(c_void_p), comp.ctypes.data_as(c_void_p), N)
    return out


def main() -> None:
    d = int(os.environ.get("F203_DECOMPRESS_D", "4"))
    if d not in (4, 10):
        raise SystemExit("F203_DECOMPRESS_D must be 4 or 10")

    os.makedirs(os.path.join(_CASE_DIR, "input"), exist_ok=True)
    os.makedirs(os.path.join(_CASE_DIR, "output"), exist_ok=True)

    rng = np.random.default_rng(SEED + d)
    max_u = (1 << d) - 1
    comp = rng.integers(0, max_u + 1, size=N, dtype=np.int32)
    golden = _decompress_ref(comp, d)

    comp.tofile(os.path.join(_CASE_DIR, "input", "comp.bin"))
    golden.tofile(os.path.join(_CASE_DIR, "output", "golden_poly.bin"))

    meta = struct.pack("<ii", N, d)
    with open(os.path.join(_CASE_DIR, "input", "meta.bin"), "wb") as f:
        f.write(meta)

    print(f"[gen_data] N={N} d={d} comp range [0,{max_u}] golden max={golden.max()}")


if __name__ == "__main__":
    main()
