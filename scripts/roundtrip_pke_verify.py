#!/usr/bin/env python3
"""roundtrip_pke_verify.py — device Decrypt 的 m.bin 与 Encrypt 输入 m.bin 字节对拍。"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

import numpy as np


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--m-device", type=Path, required=True, help="Decrypt output/m.bin")
    ap.add_argument("--m-ref", type=Path, required=True, help="Encrypt input/m.bin（明文参考）")
    args = ap.parse_args()

    m_dev = np.fromfile(args.m_device, dtype=np.uint8)
    m_ref = np.fromfile(args.m_ref, dtype=np.uint8)
    if m_dev.shape != m_ref.shape:
        print(f"[roundtrip] FAIL shape {m_dev.shape} vs {m_ref.shape}")
        sys.exit(1)
    diff = np.abs(m_dev.astype(np.int16) - m_ref.astype(np.int16))
    mx = int(diff.max()) if diff.size else 0
    if mx != 0:
        idx = int(diff.argmax())
        print(f"[roundtrip] FAIL max={mx} at {idx} device={m_dev[idx]} ref={m_ref[idx]}")
        sys.exit(1)
    print(f"[roundtrip] PASS device Decrypt(m) == Encrypt plaintext max=0 ({m_dev.size} bytes)")


if __name__ == "__main__":
    main()
