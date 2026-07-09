#!/usr/bin/env python3
"""
verify_gate.py — Decrypt 分段门禁 G1–G3：设备中间态 vs Host golden。

环境变量 DECRYPT_GATE：
  ≥1 对拍 u/v；≥2 再对拍 s_hat/u_hat；≥3 再对拍 w_hat。
生产 1-kernel fused 默认不落中间态；本脚本供多 launch / 调试路径。
仅验 I/O 系数一致，不要求与设备实现同构。
"""
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
    """逐系数 int32 对拍；失败则打印最大差位置并 exit 1。"""
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
    gate = int(os.environ.get("DECRYPT_GATE", "4"))
    if gate < 1:
        print(f"[verify_gate] skip DECRYPT_GATE={gate}")
        return

    # G1：u' / v'
    subprocess.run([sys.executable, str(HOST_GOLDEN / "gate_g1.py"), str(CASE), str(OUT)], check=True)
    cmp_int32("u", OUT / "u.bin", OUT / "golden_u.bin")
    cmp_int32("v", OUT / "v.bin", OUT / "golden_v.bin")
    print("[verify_gate] G1 u + v PASS")

    if gate < 2:
        return

    # G2：ŝ / û
    subprocess.run([sys.executable, str(HOST_GOLDEN / "gate_g2.py"), str(CASE), str(OUT)], check=True)
    cmp_int32("s_hat", OUT / "s_hat.bin", OUT / "golden_s_hat.bin")
    cmp_int32("u_hat", OUT / "u_hat.bin", OUT / "golden_u_hat.bin")
    print("[verify_gate] G2 s_hat + u_hat PASS")

    if gate < 3:
        return

    # G3：ŵ
    subprocess.run([sys.executable, str(HOST_GOLDEN / "gate_g3.py"), str(CASE), str(OUT)], check=True)
    cmp_int32("w_hat", OUT / "w_hat.bin", OUT / "golden_w_hat.bin")
    print("[verify_gate] G3 w_hat PASS")


if __name__ == "__main__":
    main()
