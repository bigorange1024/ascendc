#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RB-T07 Encrypt prep 壳（Host）：固定 ek/coins → ρ + (y, e₁, e₂)。

契约（只读 inventory G4 / FIPS 203 Alg.14 行采样段；禁抄 encrypt 源文件）：
  - ρ ← ek 尾 32 B（ML-KEM-1024 ek_PKE = 1568 B = ByteEncode₁₂(t̂)||ρ）
  - coins(r)[32] → PRF_η=SHAKE256(r‖N, 128) → SamplePolyCBD_η=2
  - nonce：y[0..3]=N0..3；e₁[0..3]=N4..7；e₂=N8（共 9 poly）

与 KeyGen SEED 路径差异（一行）：KeyGen 用 SEED_D→G→σ 再 PRF；本壳 PRF 种子直接是 coins。

Golden 积木：library/shared/fips203_se_sample/golden_se_sampling.sample_poly_cbd2
（仅调用 CBD 行函数；PRF 按 FIPS Encrypt 用 SHAKE256，不用 KeyGen 默认 shake128-shim）。
"""
from __future__ import annotations

import hashlib
import os
import sys
from pathlib import Path

import numpy as np

_SCRIPT = Path(__file__).resolve().parent
_CASE = _SCRIPT.parent
# …/graph-tests/enc_related/RB-T07-prep-shell → parents[2] = 仓库根
_REPO = _CASE.parents[2]

sys.path.insert(0, str(_REPO / "library" / "shared" / "fips203_se_sample"))
from golden_se_sampling import sample_poly_cbd2  # noqa: E402

K = 4
N = 256
ETA = 2
EK_BYTES = 1568  # ML-KEM-1024 ek_PKE
RHO_BYTES = 32
COINS_BYTES = 32
PRF_OUT = ETA * N // 4  # 128

# 固定可复现输入（非 KAT；仅壳对拍）
FIXED_COINS = bytes([(0xC0 ^ i) & 0xFF for i in range(COINS_BYTES)])
# ek：前 1536 字节确定性填充 + 尾 ρ（固定可复现，非 KAT）
FIXED_RHO = bytes([(0x50 + (i * 7)) & 0xFF for i in range(RHO_BYTES)])
FIXED_EK_HEAD = bytes([(0x11 + (i % 97)) & 0xFF for i in range(EK_BYTES - RHO_BYTES)])


def prf_shake256(coins: bytes, nonce: int) -> bytes:
    """FIPS 203 PRF_η：SHAKE256(coins‖byte(N), 64·η)。"""
    assert len(coins) == COINS_BYTES
    return hashlib.shake_256(coins + bytes([nonce & 0xFF])).digest(PRF_OUT)


def extract_rho(ek: bytes) -> bytes:
    """从 ek 尾取 ρ（32 B）。"""
    assert len(ek) == EK_BYTES
    return ek[-RHO_BYTES:]


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


def run_prep(ek: bytes, coins: bytes, out_dir: Path) -> dict:
    """
    Host 编排入口：写 ρ/y/e1/e2 bin，供后刀消费。
    @return 摘要 dict
    """
    out_dir.mkdir(parents=True, exist_ok=True)
    rho = extract_rho(ek)
    y, e1, e2 = sample_y_e1_e2(coins)

    (out_dir / "rho.bin").write_bytes(rho)
    y.astype(np.int32).tofile(out_dir / "y.bin")
    e1.astype(np.int32).tofile(out_dir / "e1.bin")
    e2.astype(np.int32).tofile(out_dir / "e2.bin")
    # 拼接便于后刀一眼对齐：y||e1||e2 → 9×256 int32
    np.concatenate([y.reshape(-1), e1.reshape(-1), e2.reshape(-1)]).astype(np.int32).tofile(
        out_dir / "y_e1_e2.bin"
    )
    return {
        "rho_hex": rho.hex(),
        "y_shape": list(y.shape),
        "e1_shape": list(e1.shape),
        "e2_shape": list(e2.shape),
        "y_sum": int(y.sum()),
        "e1_sum": int(e1.sum()),
        "e2_sum": int(e2.sum()),
    }


def main() -> int:
    inp = _CASE / "input"
    out = _CASE / "output"
    inp.mkdir(parents=True, exist_ok=True)
    out.mkdir(parents=True, exist_ok=True)

    ek = FIXED_EK_HEAD + FIXED_RHO
    assert len(ek) == EK_BYTES
    coins = FIXED_COINS

    (inp / "ek.bin").write_bytes(ek)
    (inp / "coins.bin").write_bytes(coins)

    # golden：同路径再算一遍写入 golden_*（verify 对拍）
    rho = extract_rho(ek)
    y, e1, e2 = sample_y_e1_e2(coins)
    (out / "golden_rho.bin").write_bytes(rho)
    y.astype(np.int32).tofile(out / "golden_y.bin")
    e1.astype(np.int32).tofile(out / "golden_e1.bin")
    e2.astype(np.int32).tofile(out / "golden_e2.bin")

    summary = run_prep(ek, coins, out)
    print(f"[prep_host] ek={EK_BYTES} coins={COINS_BYTES} ρ={RHO_BYTES} → y/e1/e2")
    print(f"[prep_host] summary={summary}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
