#!/usr/bin/env python3
# coding=utf-8
"""AE-E gen_data：核输入仅 ek|m|coins + NTT 表；权威 golden=c≡liboqs。

设备完成 CBD/SampleNTT/ByteDecode/Encrypt；Host 不写 ahat/that/y/e 作核输入。
假绿防护：golden 来自 liboqs_pke_ref，与设备核不同源。
"""
from __future__ import annotations

import os
import re
import sys
from pathlib import Path

import numpy as np

CASE = Path(__file__).resolve().parents[1]
REPO = CASE.parents[2]
SEED_D = int(os.environ.get("SEED_D", "20260619"))

sys.path.insert(0, str(REPO / "scripts"))
from liboqs_pke_fixture import generate_fixture  # noqa: E402

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


def main() -> None:
    fix = CASE / "input" / "liboqs_fixture"
    generate_fixture(fix, SEED_D)
    ek = (fix / "ek_pke.bin").read_bytes()
    m = (fix / "m.bin").read_bytes()
    coins = (fix / "coins.bin").read_bytes()
    c_liboqs = (fix / "c.bin").read_bytes()

    # 接线前交叉：host golden ≡ liboqs（不喂核）
    c_host = golden_encrypt(ek, m, coins)
    if c_host != c_liboqs:
        raise SystemExit("[AE-E] host golden_encrypt ≠ liboqs c — 中止")

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

    (inp / "ek_pke.bin").write_bytes(ek)
    (inp / "coins.bin").write_bytes(coins)
    np.frombuffer(m, dtype=np.uint8).tofile(inp / "m.bin")
    roots.tofile(inp / "roots.bin")
    indices.tofile(inp / "indices.bin")
    inv_roots.tofile(inp / "inv_roots.bin")
    inv_indices.tofile(inp / "inv_indices.bin")
    gammas.tofile(inp / "gammas.bin")
    Path(out / "golden_c.bin").write_bytes(c_liboqs)
    print(f"[AE-E] SEED_D={SEED_D} DEVICE_FULL: ek|m|coins+tables; golden_c=liboqs")


if __name__ == "__main__":
    main()
