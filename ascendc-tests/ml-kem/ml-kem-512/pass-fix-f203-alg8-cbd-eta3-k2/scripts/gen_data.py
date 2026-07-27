#!/usr/bin/env python3
# coding=utf-8
"""Alg.8 CBD η=3 探针 golden：prf_out[4,192] → src[4,256] int32（ML-KEM-512，k=2）。

本文件在流水线中的位置：`run.sh` 在编译完成、kernel 启动前调用它生成输入与
期望输出。输入是 4 行 × 192B 的伪随机 PRF 字节流（Alg.8 的输入契约就是 PRF
输出字节，不在本探针内复现 PRF 派生）；golden 调用共享 Python oracle
`golden_se_sampling.sample_poly_cbd3`。共享 C 侧 `fips203_sample_poly_cbd3_row`
与该 Python oracle 同语义，作为后续 C/Host 集成时的同源基准。

与 AscendC 的关系：golden 仅提供合法输入与期望输出；设备实现可使用 load24、
SWAR 与 LUT，只要求最终 `output/src.bin` 与 `output/golden_src.bin` 精确一致。
"""
from __future__ import annotations

import os
import sys
from pathlib import Path

import numpy as np


def _ascendc_repo_root(start: Path) -> Path:
    """自 start 向上查找含 AGENTS.md 与 scripts/ 的仓库根，兼容参数组多层目录。"""
    p = start.resolve()
    for d in [p, *p.parents]:
        if (d / "AGENTS.md").is_file() and (d / "scripts").is_dir():
            return d
    raise RuntimeError(f"cannot locate ascendc repo root from {start}")


ROOT = Path(__file__).resolve().parent.parent
REPO = _ascendc_repo_root(ROOT)
sys.path.insert(0, str(REPO / "library/shared/fips203_se_sample"))
from golden_se_sampling import sample_poly_cbd3  # noqa: E402

ROWS = 4          # T-B2 polyvec4：s0,s1,e0,e1
PRF_BYTES = 192   # η=3：3*256/4
N = 256           # 每个多项式系数个数
SEED = int(os.environ.get("SEED_D", "20260619"))


def main() -> None:
    """生成 PRF 输入并逐行调用 CBD_3 golden，落盘 input/prf_out.bin 与 output/golden_src.bin。"""
    rng = np.random.default_rng(SEED)
    # 直接生成 Alg.8 输入域的 PRF 字节；行与设备/Host 契约保持 [4,192] 行优先。
    prf = rng.integers(0, 256, size=ROWS * PRF_BYTES, dtype=np.uint8)
    src = np.zeros((ROWS, N), dtype=np.int32)
    for row in range(ROWS):
        # sample_poly_cbd3 内部执行 load24 + 0x00249249 抽取，并把负差映射到 [0,q)。
        chunk = bytes(prf[row * PRF_BYTES : (row + 1) * PRF_BYTES])
        src[row] = sample_poly_cbd3(chunk)

    inp = ROOT / "input"
    out = ROOT / "output"
    inp.mkdir(parents=True, exist_ok=True)
    out.mkdir(parents=True, exist_ok=True)
    prf.tofile(inp / "prf_out.bin")
    src.tofile(out / "golden_src.bin")
    print(f"[gen_data] SEED_D={SEED} prf={ROWS}x{PRF_BYTES}B src={ROWS}x{N} int32")


if __name__ == "__main__":
    main()
