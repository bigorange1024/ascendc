#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RB-K02 gen_data：Host 自算 ŝ/ê（对齐 K01 SEED_D）+ ζ；golden = NTT(ŝ)、NTT(ê)。

契约：
  - ŝ/ê：SEED_D=20260619 → Derand → G → σ → CBD_η2（golden_se_sampling.build_src）
  - NTT golden：FIPS Alg.9（复用 T10 topology_math.mlkem_ntt 作黑盒 oracle）
  - **不**把最终 NTT(ŝ)/NTT(ê) 写入 input（设备 MIX 才算）
  - ζ：只读 ntt_onnx 表，落盘供设备 NTT
  - 本刀不消费 Â；不做点积/Encode
"""
from __future__ import annotations

import os
import struct
import sys
from pathlib import Path

import numpy as np

_SCRIPT = Path(__file__).resolve().parent
_CASE = _SCRIPT.parent
_REPO = _CASE.parents[2]

SE_SHARED = _REPO / "library" / "shared" / "fips203_se_sample"
sys.path.insert(0, str(SE_SHARED))
sys.path.insert(0, str(_REPO / "graph-tests" / "enc_related" / "RB-T10-uv-device-mix" / "scripts"))

from golden_se_sampling import build_src  # noqa: E402
from topology_math import load_zetas_gammas, mlkem_ntt  # noqa: E402

K = 4
N = 256
SEED_D_DEFAULT = 20260619


def main() -> int:
    seed_d = int(os.environ.get("SEED_D", str(SEED_D_DEFAULT)))
    # 与 K01 / lines8-15 一致：PRF=SHAKE256
    os.environ["FIPS203_PRF_BACKEND"] = "shake256"

    src = build_src(seed_d).astype(np.int32)
    if src.shape != (8, N):
        raise SystemExit(f"unexpected src shape {src.shape}")
    s = src[:K].astype(np.int32)  # ŝ[4,256]
    e = src[K:].astype(np.int32)  # ê[4,256]

    zetas_list, _gammas = load_zetas_gammas()
    zetas = np.asarray(zetas_list, dtype=np.int32)

    # 极轻 Cube 输入（非零即可）
    rng = np.random.default_rng(20260909)
    mat_a = rng.integers(-8, 9, size=(16, 32), dtype=np.int8)
    mat_b = rng.integers(-8, 9, size=(32, 32), dtype=np.int8)

    inp = _CASE / "input"
    out = _CASE / "output"
    inp.mkdir(parents=True, exist_ok=True)
    out.mkdir(parents=True, exist_ok=True)

    (inp / "seed_d.bin").write_bytes(struct.pack("<I", seed_d))
    s.tofile(inp / "s.bin")
    e.tofile(inp / "e.bin")
    zetas.tofile(inp / "zetas.bin")
    mat_a.tofile(inp / "mat_a.bin")
    mat_b.tofile(inp / "mat_b.bin")

    # golden：逐 poly Alg.9（host FIPS oracle）
    s_ntt = np.stack([np.asarray(mlkem_ntt(s[i].tolist()), dtype=np.int32) for i in range(K)], axis=0)
    e_ntt = np.stack([np.asarray(mlkem_ntt(e[i].tolist()), dtype=np.int32) for i in range(K)], axis=0)
    s_ntt.tofile(out / "golden_s_ntt.bin")
    e_ntt.tofile(out / "golden_e_ntt.bin")

    print(
        f"[gen_data] SEED_D={seed_d} s={s.shape} e={e.shape} "
        f"s_ntt sum={int(s_ntt.sum())} e_ntt sum={int(e_ntt.sum())}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
