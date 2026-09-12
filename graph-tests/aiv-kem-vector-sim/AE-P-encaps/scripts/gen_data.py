#!/usr/bin/env python3
# coding=utf-8
"""AE-P gen_data：核输入仅 ek|m + NTT 表；权威 golden c/K≡liboqs。

设备同 launch：FO→K + CBD + SampleNTT + ByteDecode + Encrypt。
假绿防护：golden 来自 liboqs；Host FO 仅交叉校验，不写 output/K.bin。
"""
from __future__ import annotations

import hashlib
import os
import re
import subprocess
import sys
from pathlib import Path

import numpy as np

CASE = Path(__file__).resolve().parents[1]
REPO = CASE.parents[2]
SEED_D = int(os.environ.get("SEED_D", "20260619"))

sys.path.insert(
    0,
    str(
        REPO
        / "examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-encrypt-k4/scripts/host_golden"
    ),
)
from golden_c import golden_encrypt  # noqa: E402


def _load_array(path: Path, name: str, dtype) -> np.ndarray:
    text = path.read_text()
    m = re.search(rf"{name}\[\d+\] = \{{([^}}]+)\}}", text, re.S)
    if not m:
        raise RuntimeError(f"cannot find {name}")
    vals = [int(v.strip()) for v in m.group(1).split(",") if v.strip()]
    return np.asarray(vals, dtype=dtype)


def host_fo(ek: bytes, m: bytes) -> tuple[bytes, bytes]:
    """交叉用：K‖coins ← G(m‖H(ek))；不写设备输出。"""
    h = hashlib.sha3_256(ek).digest()
    g = hashlib.sha3_512(m + h).digest()
    return g[:32], g[32:64]


def main() -> None:
    fix = CASE / "input" / "liboqs_fixture"
    subprocess.check_call(
        [
            sys.executable,
            str(REPO / "scripts" / "liboqs_kem_fixture.py"),
            "--param",
            "1024",
            "--seed-d",
            str(SEED_D),
            "--out-dir",
            str(fix),
        ]
    )
    ek = (fix / "ek_kem.bin").read_bytes()
    m = (fix / "m.bin").read_bytes()
    c_liboqs = (fix / "c.bin").read_bytes()
    k_liboqs = (fix / "K.bin").read_bytes()

    K, coins = host_fo(ek, m)
    if K != k_liboqs:
        raise SystemExit("[AE-P] Host FO K ≠ liboqs（交叉失败）")
    c_host = golden_encrypt(ek, m, coins)
    if c_host != c_liboqs:
        raise SystemExit("[AE-P] host Encrypt ≠ liboqs c")

    tables = CASE / "generated" / "tables.hpp"
    roots = _load_array(tables, "kKemRoots", np.int32)
    inv_roots = _load_array(tables, "kKemInvRoots", np.int32)
    indices = _load_array(tables, "kKemPermutations", np.uint32)
    inv_indices = _load_array(tables, "kKemInvPermutations", np.uint32)
    gammas = _load_array(tables, "kGammas", np.int32)

    inp = CASE / "input"
    out = CASE / "output"
    inp.mkdir(exist_ok=True)
    out.mkdir(exist_ok=True)

    (inp / "ek_kem.bin").write_bytes(ek)
    np.frombuffer(m, dtype=np.uint8).tofile(inp / "m.bin")
    roots.tofile(inp / "roots.bin")
    indices.tofile(inp / "indices.bin")
    inv_roots.tofile(inp / "inv_roots.bin")
    inv_indices.tofile(inp / "inv_indices.bin")
    gammas.tofile(inp / "gammas.bin")
    Path(out / "golden_c.bin").write_bytes(c_liboqs)
    Path(out / "golden_K.bin").write_bytes(k_liboqs)
    print(f"[AE-P] SEED_D={SEED_D} DEVICE_FULL: ek|m+tables; golden c/K=liboqs")


if __name__ == "__main__":
    main()
