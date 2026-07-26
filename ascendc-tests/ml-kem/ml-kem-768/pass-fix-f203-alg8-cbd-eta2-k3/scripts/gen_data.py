#!/usr/bin/env python3
# coding=utf-8
"""Alg.8 CBD η=2 探针 golden：prf_out[6,128] → src[6,256] int32（ML-KEM-768，k=3）。

与 FIPS 203 SamplePolyCBD(η=2) 及 fips203_se_sample.c 同语义。

本文件在流水线中的位置：pass-fix-f203-alg8-cbd-eta2-k3 探针的 Host 侧数据生成
脚本，由 run.sh 在设备编译/运行前调用。职责：
  1. 用固定 SEED_D 播种的伪随机数生成 6 行 x 128 字节的 PRF 输出（模拟真实
     ML-KEM KeyGen 流程中 PRF(s/e-seed, N) 的输出，此处直接随机生成，不复现
     PRF 派生过程本身，因为 Alg.8 的输入契约就是任意 128B PRF 输出）；
  2. 调用登记表已验证的 golden 计算内核
     `library/shared/fips203_se_sample/golden_se_sampling.py::sample_poly_cbd2`
     逐行计算期望输出，落盘为 output/golden_src.bin；
  3. 落盘 input/prf_out.bin 供 main.cpp（Host）下发给设备核。
与 AscendC 实现的关系：golden 仅提供黑盒 oracle（合法输入 + 期望输出），设备侧
SWAR+LUT 实现（f203_cbd_eta2_sw_lut.hpp）不要求与本脚本或
golden_se_sampling.py 的算法逐行同构，只要求最终 output/src.bin 与本脚本产出
的 golden_src.bin 逐元素一致（I/O 等价）。
"""
from __future__ import annotations

import os
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
REPO = _ascendc_repo_root(ROOT)
sys.path.insert(0, str(REPO / "library/shared/fips203_se_sample"))
from golden_se_sampling import sample_poly_cbd2  # noqa: E402

ROWS = 6         # 6 行（polyvec6）：前 3 行对应 ŝ 各分量，后 3 行对应 ê 各分量（k=3）
PRF_BYTES = 128  # 单行 PRF 输出字节数：η(=2)*N(=256)/4
N = 256          # 每个多项式系数个数
SEED = int(os.environ.get("SEED_D", "20260619"))  # 与仓库其它 exp-mlkem 探针共用的默认种子


def main() -> None:
    """生成随机 PRF 输入并计算 golden：逐行调用 sample_poly_cbd2，落盘 input/output。"""
    rng = np.random.default_rng(SEED)
    # 6 行 x 128B 的伪随机字节流，模拟 PRF 输出（Alg.8 输入契约为任意合法 128B 数据）
    prf = rng.integers(0, 256, size=ROWS * PRF_BYTES, dtype=np.uint8)
    src = np.zeros((ROWS, N), dtype=np.int32)
    for row in range(ROWS):
        # 切出第 row 行的 128B PRF 输出，交给登记表验证过的 golden 内核计算该行 256 个 CBD 系数
        chunk = bytes(prf[row * PRF_BYTES : (row + 1) * PRF_BYTES])
        src[row] = sample_poly_cbd2(chunk)

    inp = ROOT / "input"
    out = ROOT / "output"
    inp.mkdir(parents=True, exist_ok=True)
    out.mkdir(parents=True, exist_ok=True)
    prf.tofile(inp / "prf_out.bin")
    src.tofile(out / "golden_src.bin")
    print(f"[gen_data] SEED_D={SEED} prf={ROWS}x{PRF_BYTES}B src={ROWS}x{N} int32")


if __name__ == "__main__":
    main()
