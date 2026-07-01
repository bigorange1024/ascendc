#!/usr/bin/env python3
"""gen_data — ByteDecode_d：round-trip 经 byteencode ref 生成 encoded + golden comp。"""
import os
import struct
import subprocess
import sys

import numpy as np
from ctypes import CDLL, c_int32, c_void_p

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_CASE_DIR = os.path.normpath(os.path.join(_SCRIPT_DIR, ".."))
_ENCODE_DIR = os.path.normpath(os.path.join(_CASE_DIR, "..", "pass-f203-byteencode-d4-d10-vec-k4"))
_DECODE_REF_C = os.path.join(_CASE_DIR, "byte_decode_d_ref.c")
_ENCODE_REF_C = os.path.join(_ENCODE_DIR, "byte_encode_d_ref.c")
_REF_SO = os.path.join(_CASE_DIR, "scripts", "libbytedecode_gen.so")

N = 256
SEED = 20260628
IN_BYTES = {4: 128, 10: 320}


def _build_ref_so() -> None:
    os.makedirs(os.path.dirname(_REF_SO), exist_ok=True)
    cmd = [
        "gcc",
        "-shared",
        "-fPIC",
        "-O2",
        "-I",
        _CASE_DIR,
        "-I",
        _ENCODE_DIR,
        "-o",
        _REF_SO,
        _DECODE_REF_C,
        _ENCODE_REF_C,
    ]
    subprocess.run(cmd, check=True, cwd=_CASE_DIR)


def _encode_ref(comp: np.ndarray, d: int) -> np.ndarray:
    _build_ref_so()
    lib = CDLL(_REF_SO)
    out = np.zeros(IN_BYTES[d], dtype=np.uint8)
    if d == 4:
        fn = lib.poly_byte_encode_d4_ref
    elif d == 10:
        fn = lib.poly_byte_encode_d10_ref
    else:
        raise SystemExit(f"unsupported d={d}")
    fn.argtypes = [c_void_p, c_void_p, c_int32]
    fn.restype = None
    fn(out.ctypes.data_as(c_void_p), comp.ctypes.data_as(c_void_p), N)
    return out


def _decode_ref(encoded: np.ndarray, d: int) -> np.ndarray:
    _build_ref_so()
    lib = CDLL(_REF_SO)
    out = np.zeros(N, dtype=np.int32)
    if d == 4:
        fn = lib.poly_byte_decode_d4_ref
    elif d == 10:
        fn = lib.poly_byte_decode_d10_ref
    else:
        raise SystemExit(f"unsupported d={d}")
    fn.argtypes = [c_void_p, c_void_p, c_int32]
    fn.restype = None
    fn(out.ctypes.data_as(c_void_p), encoded.ctypes.data_as(c_void_p), N)
    return out


def main() -> None:
    d = int(os.environ.get("F203_BYTE_DECODE_D", "4"))
    if d not in IN_BYTES:
        raise SystemExit("F203_BYTE_DECODE_D must be 4 or 10")

    os.makedirs(os.path.join(_CASE_DIR, "input"), exist_ok=True)
    os.makedirs(os.path.join(_CASE_DIR, "output"), exist_ok=True)

    rng = np.random.default_rng(SEED + d)
    max_u = (1 << d) - 1
    comp = rng.integers(0, max_u + 1, size=N, dtype=np.int32)
    encoded = _encode_ref(comp, d)
    golden = _decode_ref(encoded, d)

    if not np.array_equal(golden, comp):
        raise SystemExit("[gen_data] encode/decode ref round-trip failed")

    encoded.tofile(os.path.join(_CASE_DIR, "input", "encoded.bin"))
    comp.tofile(os.path.join(_CASE_DIR, "output", "golden_comp.bin"))

    meta = struct.pack("<iii", N, d, IN_BYTES[d])
    with open(os.path.join(_CASE_DIR, "input", "meta.bin"), "wb") as f:
        f.write(meta)

    print(f"[gen_data] N={N} d={d} in_bytes={IN_BYTES[d]} round-trip OK")


if __name__ == "__main__":
    main()
