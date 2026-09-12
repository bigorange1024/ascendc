#!/usr/bin/python3
# coding=utf-8
"""
EP06：硬门禁 — c/K vs liboqs Encaps（scripts/liboqs_kem_vs_ascendc_verify.py）。
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

import numpy as np

_REPO = Path(__file__).resolve().parents[4]
C_BYTES = 1568
K_BYTES = 32


def main() -> int:
    c = Path("output/c.bin")
    k = Path("output/K.bin")
    if not c.is_file() or c.stat().st_size != C_BYTES:
        print(f"[FAIL] c size want={C_BYTES}")
        return 1
    if not k.is_file() or k.stat().st_size != K_BYTES:
        print(f"[FAIL] K size want={K_BYTES}")
        return 1

    fx = Path("input/encaps_fixture")
    if not (fx / "c.bin").is_file() or not (fx / "K.bin").is_file():
        print("[FAIL] missing encaps_fixture c/K")
        return 1

    cmd = [
        sys.executable,
        str(_REPO / "scripts" / "liboqs_kem_vs_ascendc_verify.py"),
        "--stage",
        "encaps",
        "--fixture-dir",
        str(fx),
        "--ascendc-out",
        "output",
    ]
    print("[EP06] running", " ".join(cmd))
    rc = subprocess.call(cmd)
    if rc != 0:
        got_c = np.fromfile(c, dtype=np.uint8)
        ref_c = np.fromfile(fx / "c.bin", dtype=np.uint8)
        got_k = np.fromfile(k, dtype=np.uint8)
        ref_k = np.fromfile(fx / "K.bin", dtype=np.uint8)
        mx_c = int(np.abs(got_c.astype(np.int16) - ref_c.astype(np.int16)).max())
        mx_k = int(np.abs(got_k.astype(np.int16) - ref_k.astype(np.int16)).max())
        print(f"[EP06] HARD FAIL c_max={mx_c} K_max={mx_k}")
        return 1
    print("[EP06] HARD PASS c/K vs liboqs Encaps max=0")
    print("test pass (EP06 Encaps × liboqs)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
