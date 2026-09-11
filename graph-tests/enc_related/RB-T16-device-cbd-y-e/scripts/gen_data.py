#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RB-T16 gen_data：Host 预喂 coins（对齐 T07）+ golden y/e1/e2。

契约：
  - coins：与 T07 FIXED_COINS 同字节
  - PRF：SHAKE256(coins‖N, 128)，N=0..8
  - CBD：golden_se_sampling.sample_poly_cbd2（shared；禁抄 encrypt）
  - **不**把最终 y/e1/e2 写入 input（设备 MIX 才算）
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

K = 4
N = 256
ETA = 2
COINS_BYTES = 32
PRF_OUT = ETA * N // 4  # 128
NOISE_POLYS = 9

# 与 T07 prep_host.FIXED_COINS 对齐
FIXED_COINS = bytes([(0xC0 ^ i) & 0xFF for i in range(COINS_BYTES)])


def prf_shake256(coins: bytes, nonce: int) -> bytes:
    """FIPS 203 PRF_η：SHAKE256(coins‖byte(N), 64·η)。"""
    assert len(coins) == COINS_BYTES
    return hashlib.shake_256(coins + bytes([nonce & 0xFF])).digest(PRF_OUT)


def sample_y_e1_e2(coins: bytes) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """
    Alg.14：coins → y[k,256]、e1[k,256]、e2[256]（η1=η2=2）。
    @return y int32[4,256], e1 int32[4,256], e2 int32[256]
    """
    nonce = 0
    y_rows = []
    for _ in range(K):
        y_rows.append(sample_poly_cbd2(prf_shake256(coins, nonce)))
        nonce += 1
    e1_rows = []
    for _ in range(K):
        e1_rows.append(sample_poly_cbd2(prf_shake256(coins, nonce)))
        nonce += 1
    e2 = sample_poly_cbd2(prf_shake256(coins, nonce))
    return np.stack(y_rows), np.stack(e1_rows), e2


def main() -> int:
    inp = _CASE / "input"
    out = _CASE / "output"
    inp.mkdir(parents=True, exist_ok=True)
    out.mkdir(parents=True, exist_ok=True)

    coins = FIXED_COINS
    (inp / "coins.bin").write_bytes(coins)

    y, e1, e2 = sample_y_e1_e2(coins)
    yee = np.concatenate([y.reshape(-1), e1.reshape(-1), e2.reshape(-1)]).astype(np.int32)

    y.astype(np.int32).tofile(out / "golden_y.bin")
    e1.astype(np.int32).tofile(out / "golden_e1.bin")
    e2.astype(np.int32).tofile(out / "golden_e2.bin")
    yee.tofile(out / "golden_y_e1_e2.bin")

    # 极轻 Cube 输入（非零即可）
    rng = np.random.default_rng(20260908)
    mat_a = rng.integers(-8, 9, size=(16, 32), dtype=np.int8)
    mat_b = rng.integers(-8, 9, size=(32, 32), dtype=np.int8)
    mat_a.tofile(inp / "mat_a.bin")
    mat_b.tofile(inp / "mat_b.bin")

    print(
        f"[gen_data] coins={COINS_BYTES}B noise_polys={NOISE_POLYS} "
        f"y_sum={int(y.sum())} e1_sum={int(e1.sum())} e2_sum={int(e2.sum())}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
