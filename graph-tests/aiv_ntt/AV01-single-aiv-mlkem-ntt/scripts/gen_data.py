#!/usr/bin/env python3
# coding=utf-8
"""AV01：生成单 poly ML-KEM NTT 输入 / golden，并落盘根表与置换表。

golden 用独立 DFT 式 Oracle（与 thirdparty/ntt/include/reference.hpp 同构），
禁止把设备蝶形当规格。
"""
from __future__ import annotations

import os
import struct
from pathlib import Path

import numpy as np

CASE = Path(__file__).resolve().parents[1]
Q = 3329
N = 256
SEED = int(os.environ.get("AV01_SEED", "1"))


def _power(a: int, e: int, q: int) -> int:
    r = 1
    while e:
        if e & 1:
            r = r * a % q
        a = a * a % q
        e >>= 1
    return r


def _reverse(v: int, bits: int) -> int:
    r = 0
    for _ in range(bits):
        r = (r << 1) | (v & 1)
        v >>= 1
    return r


def oracle_mlkem(x: np.ndarray) -> np.ndarray:
    """独立求值：128 个二次因子，标准 bit-reversed 偶奇对序。"""
    assert x.shape == (N,)
    out = np.zeros(N, dtype=np.int32)
    for i in range(128):
        root = _power(17, 2 * _reverse(i, 7) + 1, Q)
        power = 1
        s0 = 0
        s1 = 0
        for j in range(128):
            s0 = (s0 + power * int(x[2 * j])) % Q
            s1 = (s1 + power * int(x[2 * j + 1])) % Q
            power = power * root % Q
        out[2 * i] = s0
        out[2 * i + 1] = s1
    return out


def _load_cpp_array(path: Path, name: str, dtype: str) -> np.ndarray:
    """从 generated/tables.hpp 解析 constexpr 数组（仅 AV01 抽取的 KEM 表）。"""
    text = path.read_text()
    import re

    m = re.search(rf"{name}\[\d+\] = \{{([^}}]+)\}}", text, re.S)
    if not m:
        raise RuntimeError(f"cannot find {name} in {path}")
    vals = [int(v.strip()) for v in m.group(1).split(",") if v.strip()]
    return np.asarray(vals, dtype=dtype)


def main() -> None:
    rng = np.random.default_rng(SEED)
    x = rng.integers(0, Q, size=N, dtype=np.int32)
    golden = oracle_mlkem(x)

    tables = CASE / "generated" / "tables.hpp"
    roots = _load_cpp_array(tables, "kKemRoots", np.int32)
    indices = _load_cpp_array(tables, "kKemPermutations", np.uint32)
    assert roots.shape == (1024,)
    assert indices.shape == (1792,)

    inp = CASE / "input"
    out = CASE / "output"
    inp.mkdir(exist_ok=True)
    out.mkdir(exist_ok=True)
    x.tofile(inp / "src.bin")
    roots.tofile(inp / "roots.bin")
    indices.tofile(inp / "indices.bin")
    golden.tofile(out / "golden.bin")
    print(f"[AV01] seed={SEED} wrote src/roots/indices + golden; q={Q} n={N}")


if __name__ == "__main__":
    main()
