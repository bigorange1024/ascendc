#!/usr/bin/env python3
"""liboqs_pke_vs_ascendc_verify.py — liboqs ↔ AscendC 三阶段字节对拍。"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

import numpy as np


def cmp_bytes(label: str, got: np.ndarray, ref: np.ndarray) -> None:
    if got.shape != ref.shape:
        print(f"[liboqs_vs_asc] FAIL {label}: shape {got.shape} vs {ref.shape}")
        sys.exit(1)
    diff = np.abs(got.astype(np.int16) - ref.astype(np.int16))
    mx = int(diff.max()) if diff.size else 0
    if mx != 0:
        idx = int(diff.argmax())
        print(
            f"[liboqs_vs_asc] FAIL {label}: max={mx} @ {idx} "
            f"ascendc={got.flat[idx]} liboqs={ref.flat[idx]}"
        )
        sys.exit(1)
    print(f"[liboqs_vs_asc] PASS {label} max=0 ({got.size} bytes)")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--stage", choices=("keygen", "encrypt", "decrypt"), required=True)
    ap.add_argument("--fixture", type=Path, required=True, help="liboqs fixture 目录")
    ap.add_argument("--ascendc", type=Path, required=True, help="AscendC 探针 output/ 或单文件")
    ap.add_argument("--ascendc-extra", type=Path, default=None, help="decrypt: Encrypt input/m.bin")
    args = ap.parse_args()
    fix = args.fixture.resolve()
    asc = args.ascendc.resolve()

    if args.stage == "keygen":
        cmp_bytes("ek_pke", np.fromfile(asc / "ek_pke.bin", dtype=np.uint8), np.fromfile(fix / "ek_pke.bin", dtype=np.uint8))
        cmp_bytes("dk_pke", np.fromfile(asc / "dk_pke.bin", dtype=np.uint8), np.fromfile(fix / "dk_pke.bin", dtype=np.uint8))
    elif args.stage == "encrypt":
        cmp_bytes("c.bin", np.fromfile(asc / "c.bin", dtype=np.uint8), np.fromfile(fix / "c.bin", dtype=np.uint8))
    else:
        m_dev = np.fromfile(asc / "m.bin", dtype=np.uint8)
        m_oqs = np.fromfile(fix / "m.bin", dtype=np.uint8)
        m_rec_oqs = np.fromfile(fix / "m_rec.bin", dtype=np.uint8)
        cmp_bytes("Decrypt(m) vs liboqs m", m_dev, m_oqs)
        cmp_bytes("Decrypt(m) vs liboqs m_rec", m_dev, m_rec_oqs)
        if args.ascendc_extra is not None:
            m_enc_in = np.fromfile(args.ascendc_extra.resolve(), dtype=np.uint8)
            cmp_bytes("Decrypt(m) vs Encrypt input m", m_dev, m_enc_in)


if __name__ == "__main__":
    main()
