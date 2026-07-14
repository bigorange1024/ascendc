#!/usr/bin/env python3
# coding=utf-8
"""resolve_host_seed_d.py — KEM KeyGen host SEED_D 解析（正确性定点后默认走哈希派生）。

规则：
  * 若环境变量 SEED_D 非空 → 使用该 uint32（定点复现 / 旧 KAT；如 20260619）
  * 否则 → SHA3-256(域分离标签) 前 4B 小端 → uint32，写入 seed_d.bin

密码学随机本体 d/z 仍在 device 内由 seed_d 再经 SHA3 派生；本模块只决定
host 交给设备的 4B 控制种子，避免生产默认写死字面量。
"""
from __future__ import annotations

import hashlib
import os

# 锁定标签：改字符串会改变默认 golden；勿随意改
_SEED_D_LABEL = b"ascendc:fips203-mlkem-kem-keygen-k4:host-seed-d-v1"


def resolve_host_seed_d() -> tuple[int, str]:
    """返回 (seed_d, 来源标记 env|sha3)。"""
    raw = os.environ.get("SEED_D", "").strip()
    if raw != "":
        return int(raw, 0), "env"
    digest = hashlib.sha3_256(_SEED_D_LABEL).digest()
    seed_d = int.from_bytes(digest[:4], "little")
    return seed_d, "sha3"


if __name__ == "__main__":
    sd, how = resolve_host_seed_d()
    if os.environ.get("SEED_D_RESOLVE_VERBOSE", "0") == "1":
        print(f"[resolve_host_seed_d] SEED_D={sd} via {how}", file=__import__("sys").stderr)
    print(sd)
