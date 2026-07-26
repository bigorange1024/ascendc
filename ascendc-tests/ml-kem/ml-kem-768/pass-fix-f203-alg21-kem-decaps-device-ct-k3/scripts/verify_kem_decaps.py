#!/usr/bin/env python3
"""
verify_kem_decaps.py — Decaps 输出 K 与 golden 对拍。

## 合法路径
- 比对 output/K.bin 与 golden/K.bin（gen_data 用本地 k3 D14/D19 oracle 生成）。
- 要求逐字节一致（max_abs_diff == 0）。

## Gate E3 拒绝路径
触发条件（任一）：
  - 环境变量 KEM_DECAPS_REJECT=1
  - golden/mode_reject 文件存在（gen_data REJECT 分支写入）

验收：
  1. output/K.bin == golden/K.bin（J(z‖c_bad)）
  2. output/K.bin == J(z‖c)，其中 z = dk[2368:2400]，c = input/c.bin
  3. 可选：若 golden/K_accept.bin 存在，断言 K ≠ K_accept（与合法 encaps 区分）

说明：E3 验收对象是共享密钥 K；拒绝支路直接按 FIPS 203 的 J(z‖c) 计算 golden。

## 与 M_FILE / golden_v
本脚本**不**直接读 M_FILE 或 golden_v；后者由 gen_data 在合法路径生成，供 CPU Phase-E 中间对拍。
本脚本仅验证链末 K.bin。
"""
from __future__ import annotations

import hashlib
import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def max_diff(a: bytes, b: bytes) -> int:
    """两等长字节串的最大绝对差；长度不等时返回 256 表失败。"""
    n = min(len(a), len(b))
    m = max((abs(a[i] - b[i]) for i in range(n)), default=0)
    return max(m, 256) if len(a) != len(b) else m


def main() -> int:
    out = ROOT / "output" / "K.bin"
    g = ROOT / "golden" / "K.bin"
    if not out.is_file() or not g.is_file():
        print(f"[verify] missing {out} or {g}", file=sys.stderr)
        return 2

    ob, gb = out.read_bytes(), g.read_bytes()
    if len(ob) != 32 or len(gb) != 32:
        print(f"[verify] bad size out={len(ob)} golden={len(gb)}", file=sys.stderr)
        return 1

    # 拒绝模式：env 或 gen_data 写的 mode_reject 标记
    reject = os.environ.get("KEM_DECAPS_REJECT", "0") == "1" or (ROOT / "golden" / "mode_reject").is_file()
    d = max_diff(ob, gb)
    print(f"[verify] K.bin max_abs_diff={d}")

    if reject:
        # ── Gate E3：K 必须等于 J(z‖c)，且与 golden 一致 ──
        dk = (ROOT / "input" / "dk_kem.bin").read_bytes()
        c = (ROOT / "input" / "c.bin").read_bytes()
        z = dk[2368:2400]
        j = hashlib.shake_256(z + c).digest(32)
        dj = max_diff(ob, j)
        print(f"[verify] K vs J(z||c) max_abs_diff={dj}")
        if d != 0 or dj != 0:
            print("[verify] REJECT FAIL")
            return 1
        # 与「同 dk 下合法 encaps 的 K」区分（gen 未写 K_accept 则跳过）
        k_acc = ROOT / "golden" / "K_accept.bin"
        if k_acc.is_file():
            da = max_diff(ob, k_acc.read_bytes())
            print(f"[verify] K vs accept-path K max_abs_diff={da}")
            if da == 0:
                print("[verify] REJECT FAIL: K still equals accept-path K")
                return 2
        print("[verify] REJECT PASS (device K == J(z||c))")
        return 0

    # ── 合法路径：与 encaps / Decaps golden 完全一致 ──
    print("[verify] PASS" if d == 0 else "[verify] FAIL")
    return 0 if d == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
