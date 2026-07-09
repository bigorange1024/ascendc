#!/usr/bin/env python3
"""
verify_kem.py — Alg.19 KeyGen 端到端 I/O 对拍。

比较 output/ek_kem.bin、dk_kem.bin 与 gen_data.py 产出的 golden_*；
仅验字节等价（max=0），不解释设备实现。由 run.sh 在 KEM_KEYGEN_VERIFY=1 时调用。
"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "output"


def _cmp(name: str, got_p: Path, gold_p: Path) -> None:
    """逐字节比较 got 与 golden；形状或 max_abs 非 0 则 SystemExit。"""
    if not got_p.is_file():
        raise SystemExit(f"missing output/{name}")
    if not gold_p.is_file():
        raise SystemExit(f"missing output/golden_{name} (run gen_data.py first)")
    got = np.fromfile(got_p, dtype=np.uint8)
    exp = np.fromfile(gold_p, dtype=np.uint8)
    if got.shape != exp.shape:
        raise SystemExit(f"[verify] FAIL shape {name} {got.shape} vs {exp.shape}")
    diff = np.abs(got.astype(np.int16) - exp.astype(np.int16))
    mx = int(diff.max()) if diff.size else 0
    if mx != 0:
        raise SystemExit(f"[verify] FAIL {name} max={mx}")
    print(f"[verify] PASS {name} max=0 ({got.size} bytes)")


def main() -> int:
    """对拍 ek_kem 与 dk_kem 两份产物。"""
    _cmp("ek_kem.bin", OUT / "ek_kem.bin", OUT / "golden_ek_kem.bin")
    _cmp("dk_kem.bin", OUT / "dk_kem.bin", OUT / "golden_dk_kem.bin")
    print("[verify] KEM KeyGen overall PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
