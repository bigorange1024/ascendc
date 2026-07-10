#!/usr/bin/env python3
"""
verify_result.py — Alg.15 Decrypt 生产对拍：output/m.bin vs output/golden_m.bin。

流水线位置：run.sh 在 kernel 写出 m 后调用；仅验 I/O 等价（32B 明文）。
与 golden 关系：golden_m 由 host_golden/golden_m.py（DECRYPT_VERIFY=1）生成；
本脚本不重算密码学，只做逐字节 max-abs 差。

背景：生产路径禁止中间态 D2H；验收只认 m。
"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

CASE = Path(__file__).resolve().parent.parent


def main() -> None:
    """读 m / golden_m，形状一致且 max|diff|=0 则 PASS，否则 exit 1。"""
    out = CASE / "output"
    # 设备写出的明文（32B uint8）
    m = np.fromfile(out / "m.bin", dtype=np.uint8)
    # Host oracle（与 Alg.15 语义对齐的 Python 参考）
    g = np.fromfile(out / "golden_m.bin", dtype=np.uint8)
    if m.shape != g.shape:
        print(f"[verify] FAIL shape {m.shape} vs {g.shape}")
        sys.exit(1)
    # int16 差分避免 uint8 下溢；max=0 即逐字节一致
    diff = np.abs(m.astype(np.int16) - g.astype(np.int16))
    mx = int(diff.max()) if diff.size else 0
    if mx != 0:
        idx = int(diff.argmax())
        print(f"[verify] FAIL max={mx} at {idx} m={m[idx]} g={g[idx]}")
        sys.exit(1)
    print(f"[verify] PASS max=0 ({m.size} bytes)")


if __name__ == "__main__":
    main()
