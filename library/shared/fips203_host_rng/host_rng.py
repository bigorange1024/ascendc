#!/usr/bin/env python3
# coding=utf-8
"""
fips203_host_rng — examples 侧 host 随机字节：定点可覆盖，默认哈希派生。

规则（2026-07-14）：
  * 环境变量 SEED_D 非空 → 使用该 uint32（定点 / 旧 KAT）
  * 否则 → SHA3-256(f"ascendc:{case_tag}:host-seed-d-v1")[:4] LE → uint32
  * m / coins / 其它随机块：SHAKE256(域分离标签 ‖ seed_d 十进制) 扩字节
    （取代 numpy default_rng(SEED_D+991) 写死风格）

本模块**仅** Host/golden/prepare；设备侧 d/z 仍由 device SHA3 从 seed_d 再派生。
"""
from __future__ import annotations

import hashlib
import os


def resolve_seed_d(case_tag: str, env_key: str = "SEED_D") -> tuple[int, str]:
    """
    解析 host SEED_D。

    @param case_tag 用例短名，如 fips203-mlkem-pke-encrypt-k4（进 SHA3 标签，勿随意改）
    @return (seed_d, "env"|"sha3")
    """
    raw = os.environ.get(env_key, "").strip()
    if raw != "":
        return int(raw, 0), "env"
    label = f"ascendc:{case_tag}:host-seed-d-v1".encode()
    seed_d = int.from_bytes(hashlib.sha3_256(label).digest()[:4], "little")
    return seed_d, "sha3"


def expand_bytes(case_tag: str, field: str, seed_d: int, n: int) -> bytes:
    """
    由 seed_d 经 SHAKE256 派生 n 字节随机块（m、coins 等）。

    消息 = f"ascendc:{case_tag}:{field}-v1={seed_d}"（十进制）。
    """
    msg = f"ascendc:{case_tag}:{field}-v1={seed_d}".encode()
    return hashlib.shake_256(msg).digest(n)


def main() -> None:
    import argparse

    p = argparse.ArgumentParser(description="Print resolved SEED_D for a case_tag")
    p.add_argument("case_tag")
    p.add_argument("--verbose", action="store_true")
    args = p.parse_args()
    sd, how = resolve_seed_d(args.case_tag)
    if args.verbose:
        print(f"[fips203_host_rng] SEED_D={sd} via={how}", file=__import__("sys").stderr)
    print(sd)


if __name__ == "__main__":
    main()
