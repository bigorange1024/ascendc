#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RB-T08 Host 孪生 DUT：读预喂中间量 → 拼装 u,v。

本刀无 AscendC MIX 核：拓扑数据流在 Host CPU 完成，验证
  预喂 Â/ŷ/t̂/e₁/e₂/μ → INTT(Âᵀ∘ŷ)+e₁ / INTT(⟨t̂,ŷ⟩)+e₂+μ
挂点假设（若日后接 MIX）：无 CrossCore；设备化时沿用 T03 flag 1/3+4=GATE，永禁 5/7。
"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

_SCRIPT = Path(__file__).resolve().parent
_CASE = _SCRIPT.parent
sys.path.insert(0, str(_SCRIPT))
from topology_math import K, N, compute_u_v  # noqa: E402


def main() -> int:
    inp = _CASE / "input"
    out = _CASE / "output"
    out.mkdir(parents=True, exist_ok=True)

    a_hat = np.fromfile(inp / "a_hat.bin", dtype=np.int32).reshape(K, K, N)
    y_hat = np.fromfile(inp / "y_hat.bin", dtype=np.int32).reshape(K, N)
    t_hat = np.fromfile(inp / "t_hat.bin", dtype=np.int32).reshape(K, N)
    e1 = np.fromfile(inp / "e1.bin", dtype=np.int32).reshape(K, N)
    e2 = np.fromfile(inp / "e2.bin", dtype=np.int32).reshape(N)
    mu = np.fromfile(inp / "mu.bin", dtype=np.int32).reshape(N)

    # 孪生计算段：对应日后设备侧「域拼装」挂点
    u, v = compute_u_v(a_hat, y_hat, t_hat, e1, e2, mu)

    u.astype(np.int32).tofile(out / "u.bin")
    v.astype(np.int32).tofile(out / "v.bin")
    print(f"[uv_topology_host] wrote u[{K},{N}] v[{N}] sum_u={int(u.sum())} sum_v={int(v.sum())}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
