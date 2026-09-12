#!/usr/bin/python3
# coding=utf-8
"""
EP05：R 轮不挂 + 每轮/抽样 c vs encaps max=0；抽样 K。
默认全检；EP05_CROSS_STRIDE=N 时仅检 r≡1 (mod N)。
"""

from __future__ import annotations

import os
import sys
from pathlib import Path

import numpy as np

ROUNDS = int(os.environ.get("EP05_ROUNDS", "16"))
STRIDE = int(os.environ.get("EP05_CROSS_STRIDE", "1"))  # 1=全检
C_BYTES = 1568
K_BYTES = 32


def check_round(r: int) -> bool:
    got_c = Path(f"output/r{r:02d}/c.bin")
    ref_c = Path(f"input/r{r:02d}/encaps_fixture/c.bin")
    got_k = Path(f"output/r{r:02d}/K.bin")
    ref_k = Path(f"input/r{r:02d}/encaps_fixture/K.bin")
    if not got_c.is_file() or not ref_c.is_file():
        print(f"[r{r:02d}] FAIL missing c")
        return False
    gc = np.fromfile(got_c, dtype=np.uint8)
    rc = np.fromfile(ref_c, dtype=np.uint8)
    if gc.size != C_BYTES or rc.size != C_BYTES:
        print(f"[r{r:02d}] FAIL c size")
        return False
    mx_c = int(np.abs(gc.astype(np.int16) - rc.astype(np.int16)).max())
    if mx_c != 0:
        print(f"[r{r:02d}] HARD FAIL c max={mx_c}")
        return False
    # K 抽样/全检
    if got_k.is_file() and ref_k.is_file():
        gk = np.fromfile(got_k, dtype=np.uint8)
        rk = np.fromfile(ref_k, dtype=np.uint8)
        mx_k = int(np.abs(gk.astype(np.int16) - rk.astype(np.int16)).max()) if gk.size == rk.size == K_BYTES else -1
        if mx_k != 0:
            print(f"[r{r:02d}] HARD FAIL K max={mx_k}")
            return False
        print(f"[r{r:02d}] HARD PASS c/K max=0")
    else:
        print(f"[r{r:02d}] HARD PASS c max=0 (K skip)")
    return True


if __name__ == "__main__":
    ok_all = True
    checked = 0
    for r in range(1, ROUNDS + 1):
        done = Path(f"output/round_{r:02d}_done.txt")
        if not done.is_file():
            print(f"[r{r:02d}] FAIL missing done")
            ok_all = False
            continue
        if STRIDE > 1 and ((r - 1) % STRIDE) != 0:
            print(f"[r{r:02d}] SKIP cross (stride={STRIDE})")
            continue
        checked += 1
        if not check_round(r):
            ok_all = False
    if checked == 0 or not ok_all:
        print("test FAIL: EP05 sticky")
        sys.exit(1)
    print(f"test pass (EP05 Encaps sticky R={ROUNDS} cross_checked={checked})")
    sys.exit(0)
