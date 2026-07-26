#!/usr/bin/env python3
# coding=utf-8
"""kernel 输出对拍：xof（可选）/ d1 / d2 / â[256]。

在 run.sh 于 kernel 计算结束后调用。读取 output/ 下设备写入与 gen_data.py 生成的 golden，
做逐元素相等性检查。本脚本**不**重新计算 golden，仅比对二进制文件尺寸与内容。

对拍项：
  - golden_d1.bin  vs d1.bin     — 各 D12_BYTES = 224×4 字节 int32
  - golden_d2.bin  vs d2.bin
  - golden_a_hat.bin vs a_hat.bin — 各 256×4 字节 int32
  - golden_xof.bin vs xof.bin     — 仅当 F203_ALG7_DUMP_XOF=1（调试路径，默认跳过）

失败时打印首个不匹配下标或 max_abs_diff，便于定位 XOF / d12 / rej 哪一阶段偏离。
"""
from __future__ import annotations

import os
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

from alg7_geom import D12_BYTES, XOF_BYTES  # noqa: E402

# â 输出：256 个 int32 系数
AHAT_BYTES = 256 * 4


def read_or_fail(path: Path, n: int, as_u8: bool) -> np.ndarray:
  """读取固定长度二进制文件；缺失或尺寸不符时 SystemExit。

  Args:
    path: 文件路径（output/ 下 kernel 或 golden 产物）
    n: 期望字节数
    as_u8: True → uint8 数组（xof）；False → int32 数组（d1/d2/â）
  """
  if not path.is_file():
    raise SystemExit(f"missing {path}")
  data = path.read_bytes()
  if len(data) != n:
    raise SystemExit(f"{path}: size {len(data)} != {n}")
  return np.frombuffer(data, dtype=np.uint8 if as_u8 else np.int32)


def main() -> None:
  # 与 run.sh / CMake F203_ALG7_DUMP_XOF 一致：默认不写 kernel 侧重放 xof
  dump_xof = os.environ.get("F203_ALG7_DUMP_XOF", "0") == "1"
  out = ROOT / "output"

  # 加载 golden 与 kernel 实际输出
  gd1 = read_or_fail(out / "golden_d1.bin", D12_BYTES, False)
  gd2 = read_or_fail(out / "golden_d2.bin", D12_BYTES, False)
  ga = read_or_fail(out / "golden_a_hat.bin", AHAT_BYTES, False)
  d1 = read_or_fail(out / "d1.bin", D12_BYTES, False)
  d2 = read_or_fail(out / "d2.bin", D12_BYTES, False)
  a = read_or_fail(out / "a_hat.bin", AHAT_BYTES, False)

  # 可选：xof 672B 对拍（需 kernel 开启 dump 并写出 xof.bin）
  if dump_xof:
    gx = read_or_fail(out / "golden_xof.bin", XOF_BYTES, True)
    x = read_or_fail(out / "xof.bin", XOF_BYTES, True)
    if not np.array_equal(x, gx):
      idx = int(np.argmax(x != gx))
      raise SystemExit(f"xof mismatch at {idx}: {x[idx]} vs {gx[idx]}")
    print("[verify] xof PASS")
  else:
    print("[verify] xof SKIP (F203_ALG7_DUMP_XOF=0)")

  # d1/d2：int32 精确相等（解交织阶段无浮点）
  diff1 = int(np.max(np.abs(d1.astype(np.int64) - gd1.astype(np.int64))))
  diff2 = int(np.max(np.abs(d2.astype(np.int64) - gd2.astype(np.int64))))
  diffa = int(np.max(np.abs(a.astype(np.int64) - ga.astype(np.int64))))
  if diff1 != 0 or diff2 != 0:
    raise SystemExit(f"d1 max_abs_diff={diff1} d2 max_abs_diff={diff2}")
  if diffa != 0:
    idx = int(np.argmax(a != ga))
    raise SystemExit(f"a_hat max_abs_diff={diffa} first_mismatch@{idx}: {a[idx]} vs {ga[idx]}")
  print("[verify] d1 PASS max_abs_diff=0")
  print("[verify] d2 PASS max_abs_diff=0")
  print("[verify] a_hat PASS max_abs_diff=0")


if __name__ == "__main__":
  main()
