#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RB-T10 gen_data：Host **预生成** Â、ŷ、t̂、e₁、e₂、μ、ζ、γ 与 golden_u/v；另造极轻 Cube mat。

契约（inventory G3 / FIPS；禁抄 encrypt 源文件）：
  - 输入同 T08；**不**把最终 u,v 写入 input（设备 MIX 才算）
  - golden：topology_math.compute_u_v（黑盒 I/O oracle）
  - ζ/γ：只读 ntt_onnx 表，落盘供设备 INTT / MultiplyNTTs
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
    load_zetas_gammas,
    mlkem_ntt,
    mu_embed_from_m,
)

SEED = 20260908
FIXED_M = bytes([(0xA5 if (i % 2 == 0) else 0x5A) for i in range(32)])


def main() -> int:
    inp = _CASE / "input"
    out = _CASE / "output"
    inp.mkdir(parents=True, exist_ok=True)
    out.mkdir(parents=True, exist_ok=True)

    rng = np.random.default_rng(SEED)

    def rand_hat_poly() -> np.ndarray:
        time = rng.integers(0, Q, size=N, dtype=np.int32)
        return np.asarray(mlkem_ntt(time.tolist()), dtype=np.int32)

    a_hat = np.stack(
        [[rand_hat_poly() for _ in range(K)] for _ in range(K)],
        axis=0,
    )
    y_hat = np.stack([rand_hat_poly() for _ in range(K)], axis=0)
    t_hat = np.stack([rand_hat_poly() for _ in range(K)], axis=0)

    e1 = np.mod(rng.integers(-2, 3, size=(K, N), dtype=np.int32), Q).astype(np.int32)
    e2 = np.mod(rng.integers(-2, 3, size=(N,), dtype=np.int32), Q).astype(np.int32)
    mu = mu_embed_from_m(FIXED_M)

    zetas_list, gammas_list = load_zetas_gammas()
    zetas = np.asarray(zetas_list, dtype=np.int32)
    gammas = np.asarray(gammas_list, dtype=np.int32)

    # 极轻 Cube 输入（与 T03 同形；非零即可）
    mat_a = rng.integers(-8, 9, size=(16, 32), dtype=np.int8)
    mat_b = rng.integers(-8, 9, size=(32, 32), dtype=np.int8)

    a_hat.astype(np.int32).tofile(inp / "a_hat.bin")
    y_hat.astype(np.int32).tofile(inp / "y_hat.bin")
    t_hat.astype(np.int32).tofile(inp / "t_hat.bin")
    e1.astype(np.int32).tofile(inp / "e1.bin")
    e2.astype(np.int32).tofile(inp / "e2.bin")
    mu.astype(np.int32).tofile(inp / "mu.bin")
    zetas.tofile(inp / "zetas.bin")
    gammas.tofile(inp / "gammas.bin")
    mat_a.tofile(inp / "mat_a.bin")
    mat_b.tofile(inp / "mat_b.bin")
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
