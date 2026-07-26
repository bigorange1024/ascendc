#!/usr/bin/env python3
# coding=utf-8
"""对拍 output/a_hat.bin、re.bin 与 golden（Alg.14 行 3–15 Encrypt prep）。

流水线位置：
  - 上游：scripts/gen_data.py 写 golden_a_hat.bin / golden_re.bin；main 写 a_hat.bin / re.bin
  - 本脚本：黑盒 I/O 等价验收（max_abs_diff=0），不解释设备实现细节

几何（与 f203_encrypt_prep_layout.h 一致，k=4）：
  - a_hat：16 poly × 256 系数 = 4096×int32（ρ→SampleNTT）
  - re：9 poly × 256 系数 = 2304×int32（coins→r‖e₁‖e₂，nonce 0..8）

失败时打印首个失配下标与两侧值，便于定位是 Â 还是 re 段。
"""
from __future__ import annotations

from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
KYBER_K = 4
KYBER_N = 256
# Â 矩阵扁平：k×k 个 poly
AHAT_COEFFS = KYBER_K * KYBER_K * KYBER_N
# r(4) + e1(4) + e2(1)
RE_POLYS = 2 * KYBER_K + 1
RE_COEFFS = RE_POLYS * KYBER_N


def cmp_bin(name: str, out_path: Path, golden_path: Path, expected: int) -> None:
    """逐系数 int32 对拍；expected 为期望元素个数（非字节数）。

    @param name         日志标签（a_hat / re）
    @param out_path     设备写出的 bin
    @param golden_path  gen_data 写出的 golden bin
    @param expected     系数个数（int32 元素）
    """
    ga = np.fromfile(golden_path, dtype=np.int32)
    got = np.fromfile(out_path, dtype=np.int32)
    # 先查尺寸，避免静默截断对拍
    if ga.size != expected:
        raise SystemExit(f"{name} golden size {ga.size}")
    if got.size != expected:
        raise SystemExit(f"{name} output size {got.size}")
    # 用 int64 差值避免 int32 溢出；验收门禁为严格相等
    diff = int(np.max(np.abs(got.astype(np.int64) - ga.astype(np.int64))))
    if diff != 0:
        idx = int(np.argmax(got != ga))
        raise SystemExit(f"{name} max_abs_diff={diff} first_mismatch@{idx}: {got[idx]} vs {ga[idx]}")
    print(f"[verify] {name} PASS max_abs_diff=0 ({expected} coeffs)")


def main() -> None:
    """依次对拍 a_hat、re；任一失败则 SystemExit。"""
    out = ROOT / "output"
    cmp_bin("a_hat", out / "a_hat.bin", out / "golden_a_hat.bin", AHAT_COEFFS)
    cmp_bin("re", out / "re.bin", out / "golden_re.bin", RE_COEFFS)


if __name__ == "__main__":
    main()
