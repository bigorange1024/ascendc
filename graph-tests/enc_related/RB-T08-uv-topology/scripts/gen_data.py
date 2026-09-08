#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RB-T08 gen_data：Host **预生成** Â、ŷ、t̂、e₁、e₂、μ 与 golden_u/v。

契约（inventory G3 / FIPS Alg.14 拓扑段；禁抄 encrypt 源文件）：
  - Â[K,K,N]、ŷ[K,N]、t̂[K,N] 已在 NTT 域（本刀不跑 SampleNTT/NTT 设备核）
  - e₁[K,N]、e₂[N] 时域小噪声；μ 来自 T04：m[32]→Decompress₁
  - golden：同拓扑公式 compute_u_v（黑盒 I/O oracle）
"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

_SCRIPT = Path(__file__).resolve().parent
_CASE = _SCRIPT.parent
sys.path.insert(0, str(_SCRIPT))
from topology_math import (  # noqa: E402
    K,
    N,
    Q,
    compute_u_v,
    mlkem_ntt,
    mu_embed_from_m,
)

SEED = 20260908
# 固定消息（与 T04 风格一致：交替位，覆盖 0/1665）
FIXED_M = bytes([(0xA5 if (i % 2 == 0) else 0x5A) for i in range(32)])


def main() -> int:
    inp = _CASE / "input"
    out = _CASE / "output"
    inp.mkdir(parents=True, exist_ok=True)
    out.mkdir(parents=True, exist_ok=True)

    rng = np.random.default_rng(SEED)

    # 时域随机 poly → NTT，保证 ŷ/Â/t̂ 落在合法 NTT 像上（非「任意 [0,q)」）
    def rand_hat_poly() -> np.ndarray:
        time = rng.integers(0, Q, size=N, dtype=np.int32)
        return np.asarray(mlkem_ntt(time.tolist()), dtype=np.int32)

    a_hat = np.stack(
        [[rand_hat_poly() for _ in range(K)] for _ in range(K)],
        axis=0,
    )  # [K,K,N]
    y_hat = np.stack([rand_hat_poly() for _ in range(K)], axis=0)
    t_hat = np.stack([rand_hat_poly() for _ in range(K)], axis=0)

    # CBD 风格小噪声：∈[-2,2] 映射到 Z_q
    e1 = rng.integers(-2, 3, size=(K, N), dtype=np.int32)
    e2 = rng.integers(-2, 3, size=(N,), dtype=np.int32)
    e1 = np.mod(e1, Q).astype(np.int32)
    e2 = np.mod(e2, Q).astype(np.int32)

    mu = mu_embed_from_m(FIXED_M)

    a_hat.astype(np.int32).tofile(inp / "a_hat.bin")
    y_hat.astype(np.int32).tofile(inp / "y_hat.bin")
    t_hat.astype(np.int32).tofile(inp / "t_hat.bin")
    e1.astype(np.int32).tofile(inp / "e1.bin")
    e2.astype(np.int32).tofile(inp / "e2.bin")
    mu.astype(np.int32).tofile(inp / "mu.bin")
    (inp / "m.bin").write_bytes(FIXED_M)

    u, v = compute_u_v(a_hat, y_hat, t_hat, e1, e2, mu)
    u.astype(np.int32).tofile(out / "golden_u.bin")
    v.astype(np.int32).tofile(out / "golden_v.bin")

    print(
        f"[gen_data] Â={a_hat.shape} ŷ={y_hat.shape} t̂={t_hat.shape} "
        f"e1={e1.shape} e2={e2.shape} μ ones={int(np.count_nonzero(mu))}"
    )
    print(f"[gen_data] golden u sum={int(u.sum())} v sum={int(v.sum())}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
