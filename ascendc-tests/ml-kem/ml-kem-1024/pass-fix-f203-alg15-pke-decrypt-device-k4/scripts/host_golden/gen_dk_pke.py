#!/usr/bin/env python3
"""
gen_dk_pke.py — Alg.15 输入 dk_PKE Host 夹具：ByteEncode₁₂(ŝ)（1536B）。

流水线位置：gen_data 第一步；与 Encrypt 侧 gen_ek_pke 同源 KeyGen golden（同 SEED_D）。
语义：d→(ρ,σ)→CBD 采样 s → NTT → ByteEncode₁₂ → dk（不含 ρ；ρ 在 ek 尾）。
与 golden 关系：仅造合法私钥字节；设备 decode_s_hat 再解回 ŝ。
禁止 liboqs。
"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

from gen_ek_pke import (
    K,
    N,
    POLY_D12_BYTES,
    build_a_hat,
    build_src,
    derand_bytes_from_seed,
    hash_g_rho_sigma,
    poly_byte_encode12,
    stage123_transform,
)

DK_BYTES = K * POLY_D12_BYTES  # 4×384 = 1536


def build_dk_pke(seed_d: int) -> np.ndarray:
    """
    由 SEED_D 确定性派生 dk_PKE。

    @param seed_d 与 gen_data / Encrypt 夹具共用的整数种子
    @return shape (1536,) uint8：k 个 poly 的 ByteEncode₁₂(ŝ)
    """
    # 与 KeyGen 相同的 derand → G → σ（ρ 此处不用）
    d = derand_bytes_from_seed(seed_d)
    _, sigma = hash_g_rho_sigma(d)
    # CBD2 采样 s‖e；Decrypt 私钥只要 s 的 NTT
    src = build_src(sigma)
    s = src[:K]
    s_hat = stage123_transform(s, "ntt")
    dk = bytearray(DK_BYTES)
    for j in range(K):
        dk[j * POLY_D12_BYTES : (j + 1) * POLY_D12_BYTES] = poly_byte_encode12(s_hat[j])
    return np.frombuffer(bytes(dk), dtype=np.uint8)


def main() -> None:
    """CLI：SEED_D → dk_pke.out。"""
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} <SEED_D> <dk_pke.out>", file=sys.stderr)
        sys.exit(1)
    seed_d = int(sys.argv[1])
    out = Path(sys.argv[2])
    dk = build_dk_pke(seed_d)
    if dk.size != DK_BYTES:
        raise SystemExit(f"dk size {dk.size} != {DK_BYTES}")
    out.write_bytes(dk.tobytes())
    print(f"[gen_dk_pke] OK {DK_BYTES}B SEED_D={seed_d}")


if __name__ == "__main__":
    main()
