#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RB-T13 gen_data：Host 预喂 ρ（对齐 T07 ek 尾）+ golden Â=16×SampleNTT。

契约：
  - ρ：与 T07 FIXED_RHO 同字节（ek 尾语义；禁抄 encrypt 源文件）
  - Â golden：Alg.7 SampleNTT(ρ‖j‖i)，k=4 → a_hat[16,256] 行主序（p 外 j 内）
  - **不**把最终 Â 写入 input（设备 MIX 才算）
  - XOF 672B 几何：复用活跃 alg7_geom（黑盒 oracle）
"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

_SCRIPT = Path(__file__).resolve().parent
_CASE = _SCRIPT.parent
_REPO = _CASE.parents[2]

ALG7_SCRIPTS = (
    _REPO
    / "ascendc-tests"
    / "ml-kem"
    / "ml-kem-1024"
    / "pass-fix-f203-alg7-sample-ntt-k4"
    / "scripts"
)
sys.path.insert(0, str(ALG7_SCRIPTS))

from alg7_geom import XOF_BYTES  # noqa: E402
from gen_data import (  # noqa: E402
    rej_bulk_from_d12,
    rej_scalar_from_d12,
    shake128_squeeze,
    unpack_d12_from_xof,
)

K = 4
N = 256
RHO_BYTES = 32
AHAT_POLYS = K * K
# 与 T07 prep_host.FIXED_RHO 对齐（ek 尾 32B）
FIXED_RHO = bytes([(0x50 + (i * 7)) & 0xFF for i in range(RHO_BYTES)])


def a_hat_offset(p: int, j: int) -> int:
    """行主序：Â[p,j] 扁平起始下标。"""
    return (p * K + j) * N


def sample_one_poly(rho: bytes, p: int, j: int) -> np.ndarray:
    """
    Alg.7：seed=ρ‖byte(j)‖byte(i=p) → SHAKE128 → rej → â[256]。
    @return int32[256]
    """
    seed = rho + bytes([j & 0xFF, p & 0xFF])
    xof = shake128_squeeze(seed, XOF_BYTES)
    d1, d2 = unpack_d12_from_xof(xof)
    a_spec = rej_scalar_from_d12(d1, d2)
    a_bulk = rej_bulk_from_d12(d1, d2)
    if not np.array_equal(a_spec, a_bulk):
        raise SystemExit(f"spec vs bulk mismatch at p={p} j={j}")
    return a_spec


def main() -> int:
    inp = _CASE / "input"
    out = _CASE / "output"
    inp.mkdir(parents=True, exist_ok=True)
    out.mkdir(parents=True, exist_ok=True)

    rho = FIXED_RHO
    (inp / "rho.bin").write_bytes(rho)

    a_hat = np.empty(AHAT_POLYS * N, dtype=np.int32)
    for p in range(K):
        for j in range(K):
            poly = sample_one_poly(rho, p, j)
            off = a_hat_offset(p, j)
            a_hat[off : off + N] = poly

    # 极轻 Cube 输入（非零即可）
    rng = np.random.default_rng(20260908)
    mat_a = rng.integers(-8, 9, size=(16, 32), dtype=np.int8)
    mat_b = rng.integers(-8, 9, size=(32, 32), dtype=np.int8)
    mat_a.tofile(inp / "mat_a.bin")
    mat_b.tofile(inp / "mat_b.bin")

    a_hat.tofile(out / "golden_a_hat.bin")
    (out / "golden_rho.bin").write_bytes(rho)

    print(
        f"[gen_data] ρ={RHO_BYTES}B XOF={XOF_BYTES} a_hat=({AHAT_POLYS},{N}) "
        f"sum={int(a_hat.sum())}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
