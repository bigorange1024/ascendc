#!/usr/bin/env python3
# coding=utf-8
"""Golden：Alg.13 行 8–15 SHAKE256 轨 src[8,256]；仅写 SEED_D 到 input/。"""
from __future__ import annotations

import os
import struct
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent
SCRIPTS = ROOT.parent / "fix-f203-alg13-host-scalar-fullchain-k4" / "scripts"
sys.path.insert(0, str(SCRIPTS))

from golden_se_sampling import build_src  # noqa: E402

SEED_D_DEFAULT = 20260619


def main() -> None:
    seed_d = int(os.environ.get("SEED_D", str(SEED_D_DEFAULT)))
    os.environ["FIPS203_PRF_BACKEND"] = "shake256"

    src = build_src(seed_d)
    if src.shape != (8, 256):
        raise SystemExit(f"unexpected src shape {src.shape}")

    input_dir = ROOT / "input"
    output_dir = ROOT / "output"
    input_dir.mkdir(exist_ok=True)
    output_dir.mkdir(exist_ok=True)

    (input_dir / "seed_d.bin").write_bytes(struct.pack("<I", seed_d))
    src.astype(np.int32).tofile(output_dir / "golden_src.bin")

    print(f"[gen_data] SEED_D={seed_d} FIPS203_PRF_BACKEND=shake256")
    print(f"[gen_data] wrote input/seed_d.bin + output/golden_src.bin shape={src.shape}")


if __name__ == "__main__":
    main()
