#!/usr/bin/env python3
"""
gate_g1.py — Decrypt 分段门禁 G1：由 c 生成 golden u'、v'。

对齐 FIPS 203 Alg.15 行 3–4（ByteDecode + Decompress）；仅 Host oracle，非设备规格。
用法：gate_g1.py <case_dir> <out_dir> → golden_u.bin / golden_v.bin
"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

from golden_m import unpack_ciphertext


def main() -> None:
    case = Path(sys.argv[1])
    out = Path(sys.argv[2])
    c = np.fromfile(case / "input/c.bin", dtype=np.uint8)
    u, v = unpack_ciphertext(bytes(c))
    u.tofile(out / "golden_u.bin")
    v.tofile(out / "golden_v.bin")
    print(f"[gate_g1] u={u.size} v={v.size} coeffs")


if __name__ == "__main__":
    main()
