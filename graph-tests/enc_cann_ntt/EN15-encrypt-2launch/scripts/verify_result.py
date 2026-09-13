#!/usr/bin/python3
# coding=utf-8
"""EN15：硬门禁仅 c vs liboqs max=0（中间态不得作为工程输入/软对拍依赖）。"""
from __future__ import annotations
import subprocess, sys
from pathlib import Path
import numpy as np
_REPO = Path(__file__).resolve().parents[4]

def verify_c_hard() -> bool:
    asc_c = Path("output/c.bin")
    if not asc_c.is_file():
        print("[c] FAIL missing output/c.bin")
        return False
    fixture = Path("input/liboqs_fixture")
    cmd = [sys.executable, str(_REPO / "scripts" / "liboqs_pke_vs_ascendc_verify.py"),
           "--stage", "encrypt", "--fixture", str(fixture), "--ascendc", "output"]
    print("[c] running", " ".join(cmd))
    rc = subprocess.call(cmd)
    if rc != 0:
        got = np.fromfile("output/c.bin", dtype=np.uint8)
        ref = np.fromfile(fixture / "c.bin", dtype=np.uint8)
        mx = int(np.abs(got.astype(np.int16) - ref.astype(np.int16)).max()) if got.size else -1
        print(f"[c] HARD FAIL max={mx}")
        return False
    print("[c] HARD PASS max=0")
    return True

if __name__ == "__main__":
    if not verify_c_hard():
        print("test FAIL: Encrypt c vs liboqs")
        sys.exit(1)
    print("test pass (EN15 Encrypt c ≡ liboqs)")
    sys.exit(0)
