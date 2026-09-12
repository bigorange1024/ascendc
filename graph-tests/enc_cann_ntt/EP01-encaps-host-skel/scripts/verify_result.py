#!/usr/bin/python3
# coding=utf-8
"""EP01：长度门禁 + Host H/G/K/c_stub soft 对拍。"""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

C_BYTES = 1568
K_BYTES = 32
EK_BYTES = 1568


def max_diff(a: Path, b: Path) -> int:
    ga = np.fromfile(a, dtype=np.uint8)
    gb = np.fromfile(b, dtype=np.uint8)
    if ga.size != gb.size:
        return -1
    return int(np.abs(ga.astype(np.int16) - gb.astype(np.int16)).max()) if ga.size else 0


def main() -> int:
    ek = Path("input/ek_kem.bin")
    c = Path("output/c.bin")
    k = Path("output/K.bin")
    if not ek.is_file() or ek.stat().st_size != EK_BYTES:
        print(f"[FAIL] ek size want={EK_BYTES}")
        return 1
    if not c.is_file() or c.stat().st_size != C_BYTES:
        print(f"[FAIL] c size want={C_BYTES}")
        return 1
    if not k.is_file() or k.stat().st_size != K_BYTES:
        print(f"[FAIL] K size want={K_BYTES}")
        return 1
    print(f"[LEN] PASS ek={EK_BYTES} c={C_BYTES} K={K_BYTES}")

    checks = [
        ("H", Path("output/h_ek.bin"), Path("output/golden_h.bin")),
        ("K", Path("output/K.bin"), Path("output/golden_K.bin")),
        ("r", Path("output/r.bin"), Path("output/golden_r.bin")),
        ("c_stub", Path("output/c.bin"), Path("output/golden_c_stub.bin")),
    ]
    ok = True
    for name, got, ref in checks:
        if not got.is_file() or not ref.is_file():
            print(f"[{name}] FAIL missing")
            ok = False
            continue
        mx = max_diff(got, ref)
        if mx != 0:
            print(f"[{name}] FAIL max={mx}")
            ok = False
        else:
            print(f"[{name}] PASS max=0")
    if not ok:
        print("test FAIL")
        return 1
    print("test pass (EP01 Encaps Host skel)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
