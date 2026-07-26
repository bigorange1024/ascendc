#!/usr/bin/env python3
# coding=utf-8
"""Golden：Alg.13 行 3–7 — 16× SampleNTT → a_hat[16,256]（k=4，行主序）。"""
from __future__ import annotations

import hashlib
import os
import struct
import sys
from pathlib import Path

def _ascendc_repo_root(start: Path) -> Path:
    """自 start 向上查找含 AGENTS.md 与 scripts/ 的仓库根（兼容 ml-kem 参数组嵌套）。"""
    p = start.resolve()
    for d in [p, *p.parents]:
        if (d / "AGENTS.md").is_file() and (d / "scripts").is_dir():
            return d
    raise RuntimeError(f"cannot locate ascendc repo root from {start}")


import numpy as np

ROOT = Path(__file__).resolve().parent.parent
ALG7_SCRIPTS = ROOT.parent / "pass-fix-f203-alg7-sample-ntt-k4" / "scripts"
FIPS203_SE_SCRIPTS = _ascendc_repo_root(Path(__file__).resolve()) / "library" / "shared" / "fips203_se_sample"
sys.path.insert(0, str(FIPS203_SE_SCRIPTS))
sys.path.insert(0, str(ALG7_SCRIPTS))

from alg7_geom import XOF_BYTES  # noqa: E402  # 随 F203_ALG7_XOF_504 环境变量
from golden_se_sampling import derand_bytes_from_seed  # noqa: E402

# 复用单 poly golden 的 SampleNTT 原语
from gen_data import (  # noqa: E402
  hash_g_rho,
  rej_bulk_from_d12,
  rej_scalar_from_d12,
  shake128_squeeze,
  unpack_d12_from_xof,
)

SEED_D_DEFAULT = 20260619
KYBER_K = 4
KYBER_N = 256
AHAT_POLYS = KYBER_K * KYBER_K
AHAT_BYTES = AHAT_POLYS * KYBER_N * 4


def a_hat_offset(p: int, j: int) -> int:
  return (p * KYBER_K + j) * KYBER_N


def sample_one_poly(rho: bytes, p: int, j: int) -> np.ndarray:
  seed = rho + bytes([j & 0xFF, p & 0xFF])
  xof = shake128_squeeze(seed, XOF_BYTES)
  d1, d2 = unpack_d12_from_xof(xof)
  a_spec = rej_scalar_from_d12(d1, d2)
  a_bulk = rej_bulk_from_d12(d1, d2)
  if not np.array_equal(a_spec, a_bulk):
    raise SystemExit(f"spec vs bulk mismatch at p={p} j={j}")
  return a_spec


def main() -> None:
  seed_d = int(os.environ.get("SEED_D", str(SEED_D_DEFAULT)))
  d = derand_bytes_from_seed(seed_d)
  rho = hash_g_rho(d)

  a_hat = np.empty(AHAT_POLYS * KYBER_N, dtype=np.int32)
  for p in range(KYBER_K):
    for j in range(KYBER_K):
      poly = sample_one_poly(rho, p, j)
      off = a_hat_offset(p, j)
      a_hat[off : off + KYBER_N] = poly

  input_dir = ROOT / "input"
  output_dir = ROOT / "output"
  input_dir.mkdir(exist_ok=True)
  output_dir.mkdir(exist_ok=True)

  (input_dir / "seed_d.bin").write_bytes(struct.pack("<I", seed_d))
  a_hat.tofile(output_dir / "golden_a_hat.bin")
  (output_dir / "golden_rho.bin").write_bytes(rho)

  print(f"[gen_data] SEED_D={seed_d} XOF_BYTES={XOF_BYTES} a_hat shape=({AHAT_POLYS},{KYBER_N}) bytes={AHAT_BYTES}")


if __name__ == "__main__":
  main()
