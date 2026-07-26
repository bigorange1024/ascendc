#!/usr/bin/env python3
# coding=utf-8
"""对拍：V1 prf_out；V2+ src.bin vs golden / C ref（SHAKE256）。"""
from __future__ import annotations

import ctypes
import os
import struct
import subprocess
import sys
from pathlib import Path

def _ascendc_repo_root(start: Path) -> Path:
    """自 start 向上查找含 AGENTS.md 与 scripts/ 的仓库根（兼容 ml-kem 参数组嵌套）。"""
    p = start.resolve()
    for d in [p, *p.parents]:
        if (d / "AGENTS.md").is_file() and (d / "scripts").is_dir():
            return d
    raise RuntimeError(f"cannot locate ascendc repo root from {start}")


import numpy as np

ROOT = Path(__file__).resolve().parent
REPO = _ascendc_repo_root(ROOT)
SE_SHARED = REPO / "library" / "shared" / "fips203_se_sample"
SHA3_ROOT = REPO / "thirdparty" / "tiny_sha3"
FIPS203_SE_SCRIPTS = _ascendc_repo_root(Path(__file__).resolve()) / "library" / "shared" / "fips203_se_sample"
sys.path.insert(0, str(FIPS203_SE_SCRIPTS))

from golden_se_sampling import derand_bytes_from_seed, hash_g_sigma, prf_shake256  # noqa: E402


def build_prf_out_py(seed_d: int) -> np.ndarray:
    sigma = hash_g_sigma(derand_bytes_from_seed(seed_d))
    rows = [np.frombuffer(prf_shake256(sigma, nonce), dtype=np.uint8) for nonce in range(8)]
    return np.stack(rows)


def load_seed_d() -> int:
    return struct.unpack("<I", (ROOT / "input" / "seed_d.bin").read_bytes())[0]


def check_bytes(label: str, got: np.ndarray, ref: np.ndarray) -> int:
    if got.shape != ref.shape:
        print(f"[FAIL] {label} shape {got.shape} vs {ref.shape}")
        return 1
    if not np.array_equal(got, ref):
        diff = got != ref
        idx = int(np.argmax(diff.reshape(-1)))
        flat_g = got.reshape(-1)
        flat_r = ref.reshape(-1)
        print(f"[FAIL] {label} byte mismatch idx={idx} got={int(flat_g[idx])} ref={int(flat_r[idx])}")
        return 1
    print(f"[SUCCESS] {label} byte-equal")
    return 0


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


def verify_prf(seed_d: int) -> int:
    golden = np.fromfile(ROOT / "output" / "golden_prf_out.bin", dtype=np.uint8).reshape(8, 128)
    got = np.fromfile(ROOT / "output" / "prf_out.bin", dtype=np.uint8).reshape(8, 128)
    rc = 0
    rc |= check_bytes("Device prf_out vs Python golden_prf_out (SHAKE256)", got, golden)
    py_prf = build_prf_out_py(seed_d)
    rc |= check_bytes("Device prf_out vs Python build_prf_out (SHAKE256)", got, py_prf)
    return rc


def verify_src(seed_d: int) -> int:
    golden = np.fromfile(ROOT / "output" / "golden_src.bin", dtype=np.int32).reshape(8, 256)
    got = np.fromfile(ROOT / "output" / "src.bin", dtype=np.int32).reshape(8, 256)
    rc = 0
    rc |= check_src("Device src vs Python golden_src (SHAKE256)", got, golden)
    rc |= check_src("Device src vs C fips203_build_src_shake256", got, c_ref_src(seed_d))
    return rc


def main() -> int:
    seed_d = load_seed_d()
    stage = os.environ.get("VERIFY_STAGE", "src").lower()
    if stage == "prf":
        rc = verify_prf(seed_d)
    elif stage == "src":
        rc = verify_src(seed_d)
    elif stage == "all":
        rc = verify_prf(seed_d)
        rc |= verify_src(seed_d)
    else:
        print(f"[FAIL] unknown VERIFY_STAGE={stage} (use prf|src|all)")
        return 1

    if rc == 0:
        print(f"[verify] PASS SEED_D={seed_d} stage={stage}")
    return rc


if __name__ == "__main__":
    sys.exit(main())
