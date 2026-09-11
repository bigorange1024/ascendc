#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RB-T12 gen_data：Host 自算 y（CBD/coins，接 T07 prep）+ ζ；golden_ŷ=NTT(y)。

契约：
  - y：coins→PRF_η=SHAKE256→SamplePolyCBD_η=2（同 T07；禁抄 encrypt 源文件）
  - ŷ golden：FIPS Alg.9（复用 T10 topology_math.mlkem_ntt 作黑盒 oracle）
  - **不**把最终 ŷ 写入 input（设备 MIX 才算）
  - ζ：只读 ntt_onnx 表，落盘供设备 NTT
"""
from __future__ import annotations

import hashlib
import sys
from pathlib import Path

import numpy as np

_SCRIPT = Path(__file__).resolve().parent
_CASE = _SCRIPT.parent
_REPO = _CASE.parents[2]

sys.path.insert(0, str(_REPO / "library" / "shared" / "fips203_se_sample"))
from golden_se_sampling import sample_poly_cbd2  # noqa: E402

sys.path.insert(0, str(_REPO / "graph-tests" / "enc_related" / "RB-T10-uv-device-mix" / "scripts"))
from topology_math import load_zetas_gammas, mlkem_ntt  # noqa: E402

K = 4
N = 256
ETA = 2
COINS_BYTES = 32
PRF_OUT = ETA * N // 4  # 128

# 与 T07 同可复现 coins（非 KAT）
FIXED_COINS = bytes([(0xC0 ^ i) & 0xFF for i in range(COINS_BYTES)])


def prf_shake256(coins: bytes, nonce: int) -> bytes:
    """FIPS 203 PRF_η：SHAKE256(coins‖byte(N), 64·η)。"""
    assert len(coins) == COINS_BYTES
    return hashlib.shake_256(coins + bytes([nonce & 0xFF])).digest(PRF_OUT)


def sample_y(coins: bytes) -> np.ndarray:
    """
    Alg.14 行采样段子集：coins → y[k,256]（η=2；nonce 0..3）。
    @return y int32[4,256]
    """
    rows = []
    for nonce in range(K):
        rows.append(sample_poly_cbd2(prf_shake256(coins, nonce)))
    return np.stack(rows).astype(np.int32)


def main() -> int:
    inp = _CASE / "input"
    out = _CASE / "output"
    inp.mkdir(parents=True, exist_ok=True)
    out.mkdir(parents=True, exist_ok=True)

    coins = FIXED_COINS
    (inp / "coins.bin").write_bytes(coins)

    y = sample_y(coins)
    zetas_list, _gammas = load_zetas_gammas()
    zetas = np.asarray(zetas_list, dtype=np.int32)

    # 极轻 Cube 输入（非零即可）
    rng = np.random.default_rng(20260908)
    mat_a = rng.integers(-8, 9, size=(16, 32), dtype=np.int8)
    mat_b = rng.integers(-8, 9, size=(32, 32), dtype=np.int8)

    y.tofile(inp / "y.bin")
    zetas.tofile(inp / "zetas.bin")
    mat_a.tofile(inp / "mat_a.bin")
    mat_b.tofile(inp / "mat_b.bin")

    # golden ŷ：逐 poly Alg.9
    y_hat = np.stack([np.asarray(mlkem_ntt(y[i].tolist()), dtype=np.int32) for i in range(K)], axis=0)
    y_hat.tofile(out / "golden_y_hat.bin")

    print(f"[gen_data] y={y.shape} sum={int(y.sum())} ŷ golden sum={int(y_hat.sum())}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
