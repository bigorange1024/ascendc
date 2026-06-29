#!/usr/bin/env python3
# @probe pass-fix-f203-alg13-device-keygen-k4
# @file scripts/verify_result.py
# @layer script
# @role 探针脚本：支持 golden、LUT 生成、验证或 SIM 编排。 / Helper script `verify_result.py`.
# @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
# @launch N/A（host / 脚本 / CMake 不参与 device launch）
# @ai_core N/A（非 AI Core 内核源）
# @depends Python3 标准库；可能 import 同目录 keygen_golden / numpy。
# @verify python3 调用或由 run.sh 对拍 output vs golden。

# coding=utf-8
"""G4 全链对拍：device 产物 vs Host golden。"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "output"

KYBER_K = 4
EK_POLYVEC_BYTES = KYBER_K * 384
EK_PKE_BYTES = EK_POLYVEC_BYTES + 32


def check_bytes(name: str, got: np.ndarray, golden: np.ndarray) -> int:
    if got.shape != golden.shape:
        raise SystemExit(f"{name} shape {got.shape} != {golden.shape}")
    if not np.array_equal(got, golden):
        idx = int(np.argmax(got != golden))
        raise SystemExit(f"{name} mismatch @ {idx}: {got.flat[idx]} vs {golden.flat[idx]}")
    print(f"[verify] {name} PASS (bytes={got.size})")
    return 0


def check_int32(name: str, path_out: str, path_golden: str) -> int:
    g = np.fromfile(OUT / path_golden, dtype=np.int32)
    a = np.fromfile(OUT / path_out, dtype=np.int32)
    if g.size != a.size:
        raise SystemExit(f"{name} size {a.size} != golden {g.size}")
    diff = int(np.max(np.abs(a.astype(np.int64) - g.astype(np.int64))))
    if diff != 0:
        raise SystemExit(f"{name} max_abs_diff={diff}")
    print(f"[verify] {name} PASS max_abs_diff=0")
    return 0


def main() -> int:
    rc = 0

    ek = np.fromfile(OUT / "golden_ek_polyvec.bin", dtype=np.uint8)
    rho = np.fromfile(OUT / "golden_rho.bin", dtype=np.uint8)
    ek_pke_gold = np.fromfile(OUT / "golden_ek_pke.bin", dtype=np.uint8)
    rc |= check_bytes("golden ek_pke == ek_polyvec||rho", ek_pke_gold, np.concatenate([ek, rho]))
    dk = np.fromfile(OUT / "golden_dk_pke.bin", dtype=np.uint8)
    sk = np.fromfile(OUT / "golden_sk_polyvec.bin", dtype=np.uint8)
    rc |= check_bytes("golden dk_pke == sk_polyvec", dk, sk)

    for name, out_n, gold_n in (
        ("src", "src.bin", "golden_src.bin"),
        ("a_hat", "a_hat.bin", "golden_a_hat.bin"),
    ):
        if not (OUT / out_n).is_file():
            raise SystemExit(f"missing output/{out_n}")
        rc |= check_int32(name, out_n, gold_n)

    rc |= check_bytes(
        "rho",
        np.fromfile(OUT / "rho.bin", dtype=np.uint8),
        np.fromfile(OUT / "golden_rho.bin", dtype=np.uint8),
    )

    for pair in (
        ("ek_polyvec.bin", "golden_ek_polyvec.bin"),
        ("sk_polyvec.bin", "golden_sk_polyvec.bin"),
        ("dk_pke.bin", "golden_dk_pke.bin"),
        ("ek_pke.bin", "golden_ek_pke.bin"),
    ):
        out_p, gold_p = pair
        if not (OUT / out_p).is_file():
            raise SystemExit(f"missing output/{out_p}")
        rc |= check_bytes(
            out_p.replace(".bin", ""),
            np.fromfile(OUT / out_p, dtype=np.uint8),
            np.fromfile(OUT / gold_p, dtype=np.uint8),
        )

    if not (OUT / "dst.bin").is_file():
        raise SystemExit("missing output/dst.bin")
    rc |= check_int32("dst", "dst.bin", "golden.bin")

    if rc == 0:
        print("[verify] G4 overall PASS")
    return rc


if __name__ == "__main__":
    sys.exit(main())
