#!/usr/bin/env python3
"""verify_gate.py — G1/G2/G3 中间张量对拍。"""
from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

import numpy as np

CASE = Path(__file__).resolve().parent.parent
OUT = CASE / "output"
HOST_GOLDEN = CASE / "scripts" / "host_golden"


def cmp_int32(name: str, got_path: Path, golden_path: Path) -> None:
    got = np.fromfile(got_path, dtype=np.int32)
    ref = np.fromfile(golden_path, dtype=np.int32)
    if got.shape != ref.shape:
        print(f"[verify_gate] FAIL {name} shape {got.shape} vs {ref.shape}")
        sys.exit(1)
    diff = np.abs(got.astype(np.int64) - ref.astype(np.int64))
    mx = int(diff.max()) if diff.size else 0
    if mx != 0:
        idx = int(diff.argmax())
        print(f"[verify_gate] FAIL {name} max={mx} at {idx} got={got.flat[idx]} ref={ref.flat[idx]}")
        sys.exit(1)
    print(f"[verify_gate] PASS {name} ({got.size} coeffs max=0)")


def main() -> None:
    gate = int(os.environ.get("ENCRYPT_GATE", "0"))
    if gate < 1:
        print(f"[verify_gate] skip ENCRYPT_GATE={gate}")
        return

    subprocess.run(
        [sys.executable, str(HOST_GOLDEN / "gate_g1.py"), str(CASE), str(OUT)],
        check=True,
    )

    cmp_int32("a_hat", OUT / "a_hat.bin", OUT / "golden_a_hat.bin")
    cmp_int32("r", OUT / "r.bin", OUT / "golden_r.bin")
    cmp_int32("e1", OUT / "e1.bin", OUT / "golden_e1.bin")
    cmp_int32("e2", OUT / "e2.bin", OUT / "golden_e2.bin")
    print("[verify_gate] G1 all tensors PASS")

    if gate < 2:
        return

    subprocess.run(
        [sys.executable, str(HOST_GOLDEN / "gate_g2.py"), str(CASE), str(OUT)],
        check=True,
    )
    cmp_int32("r_hat", OUT / "r_hat.bin", OUT / "golden_r_hat.bin")
    print("[verify_gate] G2 r_hat PASS")

    if gate < 3:
        return

    subprocess.run(
        [sys.executable, str(HOST_GOLDEN / "gate_g3.py"), str(CASE), str(OUT)],
        check=True,
    )
    cmp_int32("u_hat", OUT / "u_hat.bin", OUT / "golden_u_hat.bin")
    cmp_int32("tr_hat", OUT / "tr_hat.bin", OUT / "golden_tr_hat.bin")
    print("[verify_gate] G3 u_hat + tr_hat PASS")


if __name__ == "__main__":
    main()
