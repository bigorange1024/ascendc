#!/usr/bin/env python3
# coding=utf-8
"""多 SEED / (j,i) 组合 golden 抽检：规范 rej 与 bulk rej 必须逐系数一致。

在 run.sh 于 gen_data 之后、kernel 之前调用，作为**离线语义门禁**：
不依赖 AscendC，仅验证 Python golden 链路与几何常量自洽。

抽检矩阵：
  - seeds: 默认 7 个 derand 种子（含 run.sh 默认 20260619）
  - polys: 4 组 (j,i) 坐标，覆盖 j/i 非零组合

每组用例完整走通：derand → G → SHAKE128(672B) → unpack d12 → spec/bulk rej。
任一 spec!=bulk 或 â 长度≠256 即失败退出。
"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

from gen_data import (  # noqa: E402
  KYBER_N,
  hash_g_rho,
  rej_bulk_from_d12,
  rej_scalar_from_d12,
  shake128_squeeze,
  unpack_d12_from_xof,
)
from alg7_geom import XOF_BYTES  # noqa: E402

FIPS203_SE_SCRIPTS = Path(__file__).resolve().parents[3] / "library" / "shared" / "fips203_se_sample"
sys.path.insert(0, str(FIPS203_SE_SCRIPTS))
from golden_se_sampling import derand_bytes_from_seed as derand  # noqa: E402


def one_case(seed_d: int, j: int, i: int) -> None:
  """单组 (seed_d, j, i) 的完整 golden 链路与 spec==bulk 断言。"""
  d = derand(seed_d)
  rho = hash_g_rho(d)
  seed = rho + bytes([j & 0xFF, i & 0xFF])
  xof = shake128_squeeze(seed, XOF_BYTES)
  d1, d2 = unpack_d12_from_xof(xof)
  a_spec = rej_scalar_from_d12(d1, d2)
  a_bulk = rej_bulk_from_d12(d1, d2)
  if not np.array_equal(a_spec, a_bulk):
    raise SystemExit(f"FAIL seed={seed_d} j={j} i={i} spec!=bulk")
  if a_spec.shape[0] != KYBER_N:
    raise SystemExit(f"FAIL seed={seed_d} shape {a_spec.shape}")


def main() -> None:
  # 种子与 poly 坐标表：可扩展，但保持 run.sh 前置检查轻量
  seeds = [20260619, 1, 2, 42, 99, 12345, 99999]
  polys = [(0, 0), (0, 1), (1, 0), (3, 2)]
  for s in seeds:
    for j, i in polys:
      one_case(s, j, i)
  print(f"[test_multi_seed] PASS seeds={len(seeds)} polys={len(polys)} total={len(seeds)*len(polys)}")


if __name__ == "__main__":
  main()
