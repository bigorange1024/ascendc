#!/usr/bin/env python3
"""
verify_gate.py — Alg.15 Decrypt 调试门控 G1–G3：中间张量 vs Host golden。

流水线位置：非生产默认路径；由 DECRYPT_GATE 控制深度：
  ≥1 → G1：u'/v'（unpack）
  ≥2 → G2：ŝ / û（decode + NTT）
  ≥3 → G3：ŵ（su_dot / Alg.11）
生产验收仍以 verify_result（仅 m）为准；本脚本用于分段排错。

与 golden 关系：调用 host_golden/gate_g{1,2,3}.py 生成 golden_*，再 cmp int32。
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
    """
    对拍两个 int32 平面 bin：形状一致且 max|diff|=0。

    @param name 日志标签（u / v / s_hat / …）
    @param got_path 设备或夹具写出的张量
    @param golden_path Host oracle 写出的同形状张量
    """
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
    """按 DECRYPT_GATE 逐级生成 golden 并对拍；gate<1 直接跳过。"""
    gate = int(os.environ.get("DECRYPT_GATE", "4"))
    if gate < 1:
        print(f"[verify_gate] skip DECRYPT_GATE={gate}")
        return

    # ---- G1：密文 unpack → u' / v' ----
    subprocess.run([sys.executable, str(HOST_GOLDEN / "gate_g1.py"), str(CASE), str(OUT)], check=True)
    cmp_int32("u", OUT / "u.bin", OUT / "golden_u.bin")
    cmp_int32("v", OUT / "v.bin", OUT / "golden_v.bin")
    print("[verify_gate] G1 u + v PASS")

    if gate < 2:
        return

    # ---- G2：ŝ = ByteDecode₁₂(dk)；û = NTT(u') ----
    subprocess.run([sys.executable, str(HOST_GOLDEN / "gate_g2.py"), str(CASE), str(OUT)], check=True)
    cmp_int32("s_hat", OUT / "s_hat.bin", OUT / "golden_s_hat.bin")
    cmp_int32("u_hat", OUT / "u_hat.bin", OUT / "golden_u_hat.bin")
    print("[verify_gate] G2 s_hat + u_hat PASS")

    if gate < 3:
        return

    # ---- G3：ŵ = ⟨ŝ, û⟩（Alg.11 累加）----
    subprocess.run([sys.executable, str(HOST_GOLDEN / "gate_g3.py"), str(CASE), str(OUT)], check=True)
    cmp_int32("w_hat", OUT / "w_hat.bin", OUT / "golden_w_hat.bin")
    print("[verify_gate] G3 w_hat PASS")


if __name__ == "__main__":
    main()
