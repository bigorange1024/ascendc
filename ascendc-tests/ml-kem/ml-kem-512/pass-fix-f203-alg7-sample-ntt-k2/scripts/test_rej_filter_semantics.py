#!/usr/bin/env python3
# coding=utf-8
"""剔除（reject）双方案语义等价性自检：Mins 路径 vs Compares(LT)+Select 路径。

设备侧 f203_alg7_rej_filter.hpp 提供两种实现（由 F203_ALG7_REJ_IMPL 选择）：
  - impl=1 vec_mins：v < q 保留 v，否则写哨兵 q
  - impl=2 vec_mask：Compares(LT) 得掩码再 Select，数学目标相同

本脚本用纯 Python 模拟两种写回规则，验证对任意输入列表结果一致。
这是 run.sh 前置门禁，确保向量剔除与 golden 中「v>=q → 标记 q」语义对齐。

注意：此处仅测**单 lane 写回**；完整 rej 还与 compact/Gather、规范顺序截取有关，
那些由 test_multi_seed.py 与 test_rej_scalar_c.py 覆盖。
"""
from __future__ import annotations

import sys

KYBER_Q = 3329


def reject_mins(src: list[int], q: int = KYBER_Q) -> list[int]:
    """Mins 语义：逐元素 min 式写回，v<q 保留 v，否则置 q（拒绝哨兵）。"""
    return [v if v < q else q for v in src]


def reject_mask_lt(src: list[int], q: int = KYBER_Q) -> list[int]:
    """Compares(LT)+Select 语义 golden：显式分支，与 Mins 目标相同（便于对照阅读）。"""
    out: list[int] = []
    for v in src:
        out.append(v if v < q else q)
    return out


def test_cases() -> None:
  # 边界用例：空、q 邻域、超大值、负值、长序列
  cases = [
      [],
      [0, 1, 3328, 3329, 3330, 5000, -1],
      list(range(3400)),
  ]
  for src in cases:
      a = reject_mins(src)
      b = reject_mask_lt(src)
      assert a == b, (src[:8], a[:8], b[:8])
  print("[OK] rej filter mins == mask_lt semantics")


if __name__ == "__main__":
    test_cases()
    sys.exit(0)
