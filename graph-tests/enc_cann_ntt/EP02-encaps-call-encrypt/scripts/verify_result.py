#!/usr/bin/python3
# coding=utf-8
"""EP02：硬门禁 c/K 尺寸 + soft Host/encaps golden（max=0）。"""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

C_BYTES = 1568
K_BYTES = 32


def max_u8(a: Path, b: Path) -> int:
    ga = np.fromfile(a, dtype=np.uint8)
    gb = np.fromfile(b, dtype=np.uint8)
    if ga.size != gb.size:
        return -1
    return int(np.abs(ga.astype(np.int16) - gb.astype(np.int16)).max()) if ga.size else 0


def main() -> int:
    c = Path("output/c.bin")
    k = Path("output/K.bin")
    if not c.is_file() or c.stat().st_size != C_BYTES:
        print(f"[FAIL] c size want={C_BYTES}")
        return 1
    if not k.is_file() or k.stat().st_size != K_BYTES:
        print(f"[FAIL] K size want={K_BYTES}")
        return 1
    print(f"[LEN] PASS c={C_BYTES} K={K_BYTES}")

    ok = True
    for name, got, ref in (
        ("c", c, Path("output/golden_c.bin")),
        ("K", k, Path("output/golden_K.bin")),
    ):
        if not ref.is_file():
            print(f"[{name}] FAIL missing {ref}")
            ok = False
            continue
        mx = max_u8(got, ref)
        if mx != 0:
            print(f"[{name}] FAIL max={mx}")
            ok = False
        else:
            print(f"[{name}] PASS max=0")
    if not ok:
        print("test FAIL")
        return 1
    print("test pass (EP02 Encaps→Encrypt c/K)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
