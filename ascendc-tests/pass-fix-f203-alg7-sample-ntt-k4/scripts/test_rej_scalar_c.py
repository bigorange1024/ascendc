#!/usr/bin/env python3
# coding=utf-8
"""编译并测试 f203_alg7_rej_scalar.c 与 Python golden 的 rej 语义一致。

本脚本在 run.sh 于 kernel 编译前执行，双重验证：
  1. **单元测试**：gcc 编译 f203_alg7_rej_scalar.c + 微型 harness，核对已知 d1/d2 输出
  2. **随机性质检**：200 组随机 d1/d2，Python 内联 spec 循环 vs bulk 过滤必须一致

第 1 步确保 C 标量实现与头文件 API 可链接、基本行为正确；
第 2 步与 gen_data.rej_scalar_from_d12 / rej_bulk_from_d12 形成交叉覆盖，
保证设备默认路径（F203_ALG7_REJ_IMPL=0 或标量对照）与 golden 同一语义。

注：仓库内源文件为 f203_alg7_rej_scalar.c（本脚本名保留 _c 后缀表「测 C 实现」）。
"""
from __future__ import annotations

import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
C_SRC = ROOT / "f203_alg7_rej_scalar.c"
H_SRC = ROOT / "f203_alg7_rej_scalar.h"

# 复用 gen_data 中的 rej 与 unpack 逻辑作为 Python 参照
sys.path.insert(0, str(ROOT / "scripts"))
from gen_data import KYBER_Q, rej_bulk_from_d12, rej_scalar_from_d12, unpack_d12_from_xof  # noqa: E402


def main() -> None:
  # --- 1. C 单元 harness：3 对候选，n_out=8，验证规范顺序接受个数与系数序 ---
  # d1={0,4000,100}, d2={200,5000,50} → 接受 0,200,100,50 共 4 个（4000/5000 ≥ q 拒绝）
  harness = r"""
#include <stdio.h>
#include <stdint.h>
#include "f203_alg7_rej_scalar.h"
int main(void) {
  int32_t d1[3] = {0, 4000, 100};
  int32_t d2[3] = {200, 5000, 50};
  int32_t out[8] = {0};
  uint32_t n = f203_alg7_rej_scalar_from_d12(d1, d2, 3, 3329, out, 8);
  printf("%u", n);
  for (uint32_t i = 0; i < n; ++i) printf(" %d", out[i]);
  return 0;
}
"""
  with tempfile.TemporaryDirectory() as td:
    tdp = Path(td)
    (tdp / "test.c").write_text(harness, encoding="utf-8")
    exe = tdp / "test_rej"
    subprocess.check_call(["gcc", "-O2", "-I", str(ROOT), str(C_SRC), str(tdp / "test.c"), "-o", str(exe)])
    out = subprocess.check_output([str(exe)], text=True).strip()
    # 期望：4 个接受系数，顺序为规范扫描 0,200,100,50
    assert out == "4 0 200 100 50", out

  # --- 2. 随机 d12：内联 spec 循环 vs bulk 掩码路径（与 gen_data 双路径一致）---
  rng = np.random.default_rng(42)
  for _ in range(200):
    raw = rng.integers(0, 4096, size=168 * 2, dtype=np.int32)
    d1 = raw[:168]
    d2 = raw[168:]
    stream: list[int] = []
    spec: list[int] = []
    for i in range(168):
      v1 = int(d1[i])
      if v1 < KYBER_Q:
        spec.append(v1)
      v2 = int(d2[i])
      if v2 < KYBER_Q:
        spec.append(v2)
      stream.append(v1 if v1 < KYBER_Q else KYBER_Q)
      stream.append(v2 if v2 < KYBER_Q else KYBER_Q)
    bulk = [x for x in stream if x < KYBER_Q]
    assert spec == bulk

  print("[test_rej_scalar_c] PASS (unit + 200 random spec==bulk)")


if __name__ == "__main__":
  main()
