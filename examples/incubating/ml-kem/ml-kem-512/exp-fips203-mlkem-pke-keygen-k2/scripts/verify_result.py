#!/usr/bin/env python3
# @probe exp-fips203-mlkem-pke-keygen-k2
# @file scripts/verify_result.py
# @layer script
# @role 探针脚本：支持 golden、LUT 生成、验证或 SIM 编排。 / Helper script `verify_result.py`.
# @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (800B) + dk_pke.bin (768B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
# @launch N/A（host / 脚本 / CMake 不参与 device launch）
# @ai_core N/A（非 AI Core 内核源）
# @depends Python3 标准库；可能 import 同目录 keygen_golden / numpy。
# @verify python3 调用或由 run.sh 对拍 output vs golden。

# coding=utf-8
"""
本文件在 KeyGen 流水线中的位置：Host 验收脚本（G4 / 分阶段门控）。
对齐：FIPS 203 Alg.13 / ML-KEM-512（k=2）。
与 golden 关系：仅 I/O 等价（output/* vs golden_*）；禁止把本脚本当作 AscendC 规格。
文件：scripts/verify_result.py

G4 全链对拍：device 产物 vs Host golden（含中间 src/a_hat/rho 与 ek/dk）。
"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "output"

KYBER_K = 2
EK_POLYVEC_BYTES = KYBER_K * 384
EK_PKE_BYTES = EK_POLYVEC_BYTES + 32


def check_bytes(name: str, got: np.ndarray, golden: np.ndarray) -> int:
    """逐字节对拍；形状或内容不一致则 SystemExit。

    @return 0 表示通过
    """
    if got.shape != golden.shape:
        raise SystemExit(f"{name} shape {got.shape} != {golden.shape}")
    if not np.array_equal(got, golden):
        idx = int(np.argmax(got != golden))
        raise SystemExit(f"{name} mismatch @ {idx}: {got.flat[idx]} vs {golden.flat[idx]}")
    print(f"[verify] {name} PASS (bytes={got.size})")
    return 0


def check_int32(name: str, path_out: str, path_golden: str) -> int:
    """按 int32 读 output 与 golden，要求 max_abs_diff==0。

    @return 0 表示通过
    """
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
    """先自检 golden 拼接不变量，再对拍设备落盘产物。

    @return 0 全过；非 0 由 check_* 抛错退出
    """
    rc = 0

    # --- golden 自检：ek_pke = ek_polyvec‖ρ；dk_pke = sk_polyvec ---
    ek = np.fromfile(OUT / "golden_ek_polyvec.bin", dtype=np.uint8)
    rho = np.fromfile(OUT / "golden_rho.bin", dtype=np.uint8)
    ek_pke_gold = np.fromfile(OUT / "golden_ek_pke.bin", dtype=np.uint8)
    rc |= check_bytes("golden ek_pke == ek_polyvec||rho", ek_pke_gold, np.concatenate([ek, rho]))
    dk = np.fromfile(OUT / "golden_dk_pke.bin", dtype=np.uint8)
    sk = np.fromfile(OUT / "golden_sk_polyvec.bin", dtype=np.uint8)
    rc |= check_bytes("golden dk_pke == sk_polyvec", dk, sk)

    # --- prep 中间量：ŝ‖ê 与 Â ---
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

    # --- 生产/门控输出：编码公钥、私钥、ek_pke/dk_pke ---
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

    # --- NTT 后 dst（int32 polyvec）---
    if not (OUT / "dst.bin").is_file():
        raise SystemExit("missing output/dst.bin")
    rc |= check_int32("dst", "dst.bin", "golden.bin")

    if rc == 0:
        print("[verify] G4 overall PASS")
    return rc


if __name__ == "__main__":
    sys.exit(main())
