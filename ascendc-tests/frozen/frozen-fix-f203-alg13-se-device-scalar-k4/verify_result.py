#!/usr/bin/env python3
# coding=utf-8
"""对拍：Device src.bin vs Python golden / C fips203_build_src（SHAKE256）。"""
from __future__ import annotations

import ctypes
import os
import struct
import subprocess
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent
REPO = ROOT.parent.parent
SE_SHARED = REPO / "library" / "shared" / "fips203_se_sample"
SHA3_ROOT = REPO / "thirdparty" / "tiny_sha3"


def load_seed_d() -> int:
    return struct.unpack("<I", (ROOT / "input" / "seed_d.bin").read_bytes())[0]


def check_src(label: str, got: np.ndarray, ref: np.ndarray) -> int:
    if got.shape != ref.shape:
        print(f"[FAIL] {label} shape {got.shape} vs {ref.shape}")
        return 1
    diff = np.abs(got.astype(np.int64) - ref.astype(np.int64))
    mx = int(diff.max())
    if mx != 0:
        idx = int(diff.argmax())
        flat_g = got.reshape(-1)
        flat_r = ref.reshape(-1)
        print(
            f"[FAIL] {label} max_abs_diff={mx} idx={idx} "
            f"got={int(flat_g.reshape(-1)[idx])} ref={int(flat_r.reshape(-1)[idx])}"
        )
        return 1
    print(f"[SUCCESS] {label} max_abs_diff=0")
    return 0


def c_ref_src(seed_d: int) -> np.ndarray:
    build_dir = ROOT / "build_ref"
    build_dir.mkdir(exist_ok=True)
    so_path = build_dir / "libfips203_se_sample_shake256.so"
    se_files = [
        SE_SHARED / "fips203_prf.c",
        SE_SHARED / "fips203_se_sample.c",
        SHA3_ROOT / "sha3.c",
    ]
    newest = max(p.stat().st_mtime for p in se_files)
    if not so_path.is_file() or so_path.stat().st_mtime < newest:
        subprocess.run(
            [
                "gcc",
                "-shared",
                "-fPIC",
                "-O2",
                f"-I{SE_SHARED}",
                f"-I{SHA3_ROOT}",
                *[str(p) for p in se_files],
                "-o",
                str(so_path),
            ],
            check=True,
        )
    os.environ["FIPS203_PRF_BACKEND"] = "shake256"
    lib = ctypes.CDLL(str(so_path))
    lib.fips203_build_src.argtypes = [ctypes.POINTER(ctypes.c_int32), ctypes.c_uint32]
    lib.fips203_build_src.restype = ctypes.c_int
    buf = np.zeros((8, 256), dtype=np.int32)
    rc = lib.fips203_build_src(buf.ctypes.data_as(ctypes.POINTER(ctypes.c_int32)), ctypes.c_uint32(seed_d))
    if rc != 0:
        raise SystemExit(f"C fips203_build_src (shake256) rc={rc}")
    return buf


def main() -> int:
    seed_d = load_seed_d()
    golden = np.fromfile(ROOT / "output" / "golden_src.bin", dtype=np.int32).reshape(8, 256)
    got = np.fromfile(ROOT / "output" / "src.bin", dtype=np.int32).reshape(8, 256)

    rc = 0
    rc |= check_src("Device vs Python golden_src (SHAKE256)", got, golden)
    rc |= check_src("Device vs C fips203_build_src_shake256", got, c_ref_src(seed_d))

    if rc == 0:
        print(f"[verify] PASS SEED_D={seed_d}")
    return rc


if __name__ == "__main__":
    sys.exit(main())
