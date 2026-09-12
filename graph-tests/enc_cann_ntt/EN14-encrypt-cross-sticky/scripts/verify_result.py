#!/usr/bin/python3
# coding=utf-8
"""
EN14：硬门禁 — 每轮 output/rXX/c.bin ≡ input/rXX/liboqs_fixture/c.bin（max=0）。
"""

from __future__ import annotations

import os
import sys
from pathlib import Path

import numpy as np

ROUNDS = int(os.environ.get("EN14_ROUNDS", "8"))
C_BYTES = 1568


def check_round(r: int) -> bool:
    got_p = Path(f"output/r{r:02d}/c.bin")
    ref_p = Path(f"input/r{r:02d}/liboqs_fixture/c.bin")
    if not got_p.is_file():
        print(f"[r{r:02d}] FAIL missing {got_p}")
        return False
    if not ref_p.is_file():
        print(f"[r{r:02d}] FAIL missing {ref_p}")
        return False
    got = np.fromfile(got_p, dtype=np.uint8)
    ref = np.fromfile(ref_p, dtype=np.uint8)
    if got.size != C_BYTES or ref.size != C_BYTES:
        print(f"[r{r:02d}] FAIL size got={got.size} ref={ref.size} want={C_BYTES}")
        return False
    diff = np.abs(got.astype(np.int16) - ref.astype(np.int16))
    mx = int(diff.max()) if diff.size else -1
    bad = int(np.count_nonzero(diff))
    if mx != 0:
        idx = int(np.argmax(diff))
        print(f"[r{r:02d}] HARD FAIL max={mx} bad={bad} first@{idx}")
        return False
    print(f"[r{r:02d}] HARD PASS max=0 ({C_BYTES} bytes)")
    return True


if __name__ == "__main__":
    if ROUNDS < 1:
        print("EN14_ROUNDS invalid")
        sys.exit(2)
    ok_all = True
    for r in range(1, ROUNDS + 1):
        done = Path(f"output/round_{r:02d}_done.txt")
        if not done.is_file():
            print(f"[r{r:02d}] FAIL missing {done}")
            ok_all = False
            continue
        if not check_round(r):
            ok_all = False
    if not ok_all:
        print("test FAIL: EN14 sticky c vs liboqs")
        sys.exit(1)
    print(f"test pass (EN14 Encrypt sticky R={ROUNDS} c ≡ liboqs each round)")
    sys.exit(0)
