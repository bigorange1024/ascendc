#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""gen_data — RB-D02 Decrypt L2a：造 u/ŝ 与 golden û/ŵ。

Host oracle（FIPS 203 契约，非 liboqs 权威）：
  - u：随机 [0,q) 时域 polyvec k=4
  - ŝ：随机 [0,q) NTT 域 polyvec（Decrypt 私钥侧已是 NTT）
  - û = NTT(u) 逐 poly（Alg.9）
  - ŵ = Σ_j MultiplyNTTs(ŝ[j], û[j])（Alg.11）
ζ/γ：只读 ntt_onnx 表（via T10 topology_math）。
禁把最终 û/ŵ 写入 input（设备 MIX 才算）。
"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

_SCRIPT = Path(__file__).resolve().parent
_CASE = _SCRIPT.parent
_REPO = _CASE.parents[2]

# Host oracle：复用 enc_related T10 数学积木（仅 golden；非设备规格）
sys.path.insert(0, str(_REPO / "graph-tests" / "enc_related" / "RB-T10-uv-device-mix" / "scripts"))
from topology_math import (  # noqa: E402
    Q,
    K,
    N,
    load_zetas_gammas,
    mlkem_ntt,
    multiply_ntts,
    mod_q,
)

SEED = 20260909


def main() -> int:
    inp = _CASE / "input"
    out = _CASE / "output"
    inp.mkdir(parents=True, exist_ok=True)
    out.mkdir(parents=True, exist_ok=True)

    rng = np.random.default_rng(SEED)

    # u：时域；ŝ：已是 NTT 域（与 D01 写出语义一致）
    u = rng.integers(0, Q, size=(K, N), dtype=np.int32)
    s_hat = rng.integers(0, Q, size=(K, N), dtype=np.int32)

    zetas_list, gammas_list = load_zetas_gammas()
    zetas = np.asarray(zetas_list, dtype=np.int32)
    gammas = np.asarray(gammas_list, dtype=np.int32)

    # 极轻 Cube 输入（非零即可）
    mat_a = rng.integers(-8, 9, size=(16, 32), dtype=np.int8)
    mat_b = rng.integers(-8, 9, size=(32, 32), dtype=np.int8)

    u.tofile(inp / "u.bin")
    s_hat.tofile(inp / "s_hat.bin")
    zetas.tofile(inp / "zetas.bin")
    gammas.tofile(inp / "gammas.bin")
    mat_a.tofile(inp / "mat_a.bin")
    mat_b.tofile(inp / "mat_b.bin")

    # golden û：逐 poly Alg.9
    u_hat = np.stack(
        [np.asarray(mlkem_ntt(u[i].tolist()), dtype=np.int32) for i in range(K)], axis=0
    )

    # golden ŵ：Σ MultiplyNTTs(ŝ[j], û[j])
    acc = np.zeros(N, dtype=np.int64)
    for j in range(K):
        prod = multiply_ntts(s_hat[j].tolist(), u_hat[j].tolist())
        acc += np.asarray(prod, dtype=np.int64)
    w_hat = np.array([mod_q(int(x)) for x in acc], dtype=np.int32)

    u_hat.tofile(out / "golden_u_hat.bin")
    w_hat.tofile(out / "golden_w_hat.bin")

    print(
        f"[gen_data] u={u.shape} ŝ={s_hat.shape} "
        f"û sum={int(u_hat.sum())} ŵ sum={int(w_hat.sum())}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
