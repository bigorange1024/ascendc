#!/usr/bin/env python3
# @probe pass-fix-f203-alg13-device-keygen-k4
# @file scripts/prep/gen_data.py
# @layer script
# @role 探针脚本：支持 golden、LUT 生成、验证或 SIM 编排。 / Helper script `gen_data.py`.
# @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
# @launch N/A（host / 脚本 / CMake 不参与 device launch）
# @ai_core N/A（非 AI Core 内核源）
# @depends Python3 标准库；可能 import 同目录 keygen_golden / numpy。
# @verify run.sh 编译前生成 input/；KEYGEN_VERIFY=1 时参与对拍。

# coding=utf-8
"""
本文件在 KeyGen 流水线中的位置：Host：prep 段 golden / ROM 生成脚本。
对齐：FIPS 203 Alg.13 / ML-KEM-1024（k=4）。
与 golden 关系：仅 I/O 等价验收；禁止把 Host/参考源码当作 AscendC 实现规格。
文件：scripts/prep/gen_data.py
"""

from __future__ import annotations

"""Golden 生成器：Alg.7 单 poly SampleNTT — xof[672]、d1/d2[224]、â[256]。

本脚本是探针 pass-fix-f203-alg7-sample-ntt-k4 的**黑盒 oracle**：
仅负责按 FIPS 203 / Kyber 语义生成合法输入与期望输出，供 run.sh + verify_result.py 对拍。
**禁止**把本脚本的实现细节当作 AscendC kernel 必须逐步复刻的规格。

流水线（与 main.cpp / f203_alg7_sample_ntt_entry 一致）：
  1. SEED_D → derand → 32B 种子 d
  2. ρ = G(d) = SHA3-512(d || byte(k)) 前 32B，k=4（ML-KEM）
  3. sample_seed = ρ || byte(j) || byte(i)，由环境变量 ALG7_POLY_J / ALG7_POLY_I 指定
  4. xof = SHAKE128(sample_seed).squeeze(XOF_BYTES)，固定 672B（见 alg7_geom / f203_alg7_layout.h）
  5. 三字节解交织 → d1[224], d2[224]（Alg.7 步骤 6–7 的全量 triple，无早停）
  6. rej 采样 → â[256]（模 q=3329，规范顺序边扫边填）

输出文件（ROOT/input 与 ROOT/output）：
  - input/seed_d.bin      — uint32 LE，host 传入 kernel 的 derand 种子
  - input/poly_ij.bin     — 2B：[j, i]
  - output/golden_xof.bin — 672B uint8
  - output/golden_d1.bin  — 224×int32
  - output/golden_d2.bin  — 224×int32
  - output/golden_a_hat.bin — 256×int32，NTT 域系数 â

自检：同一 (d1,d2) 上 spec（规范顺序 rej）与 bulk（全量掩码+过滤）必须一致。
"""

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

# 探针根目录与共享 golden 工具（derand 来自 alg13 全链探针的 golden_se_sampling）
ROOT = Path(__file__).resolve().parent.parent
FIPS203_SE_SCRIPTS = _ascendc_repo_root(Path(__file__).resolve()) / "library" / "shared" / "fips203_se_sample"
sys.path.insert(0, str(FIPS203_SE_SCRIPTS))
sys.path.insert(0, str(ROOT / "scripts"))

from alg7_geom import CAND_PAIRS, XOF_BYTES  # noqa: E402
from golden_se_sampling import derand_bytes_from_seed  # noqa: E402

# 默认 derand 种子（与 run.sh SEED_D 默认一致）
SEED_D_DEFAULT = 20260619
# ML-KEM / Kyber 参数（本探针单 poly，k 仅用于 G(d) 哈希后缀）
KYBER_K = 4
KYBER_N = 256
KYBER_Q = 3329


def hash_g_rho(d: bytes, k: int = KYBER_K) -> bytes:
  """G(d) → ρ：SHA3-512(d || byte(k)) _digest 的前 32 字节。

  对应 FIPS 203 / Kyber 的矩阵种子扩展；k 为安全参数（ML-KEM-768/1024 等用 k=3/4）。
  """
  return hashlib.sha3_512(d + bytes([k & 0xFF])).digest()[:32]


def shake128_squeeze(msg: bytes, outlen: int) -> bytes:
  """在同一次 SHAKE128 absorb 上连续 squeeze outlen 字节。

  语义等价于 Kyber 的「首批多块 squeeze + 不足时每次再 squeeze 1×rate」，
  本探针用固定 outlen=XOF_BYTES(672) 一次取齐，避免 lazy tail 分支。
  """
  return hashlib.shake_128(msg).digest(outlen)


def unpack_d12_from_xof(buf: bytes) -> tuple[np.ndarray, np.ndarray]:
  """将 xof[672] 三字节解交织为 d1[224], d2[224]（Alg.7 步骤 6–7，全量 triple）。

  每个三元组 (c0, c1, c2) 的位布局（与 Kyber Parse 一致）：
    d1 = c0 + 256 * (c1 & 0x0F)      — 低 12 bit 有效，范围 [0, 4095]
    d2 = (c1 >> 4) + 16 * c2         — 低 12 bit 有效
  不做 rej；设备侧 d12 阶段输出应与该全量数组一致。
  """
  if len(buf) != XOF_BYTES:
    raise ValueError(f"expected {XOF_BYTES} bytes, got {len(buf)}")
  d1 = np.empty(CAND_PAIRS, dtype=np.int32)
  d2 = np.empty(CAND_PAIRS, dtype=np.int32)
  pos = 0
  for t in range(CAND_PAIRS):
    c0, c1, c2 = buf[pos], buf[pos + 1], buf[pos + 2]
    # 12-bit 小端拼装：d1 取 c0 全字节 + c1 低 nibble；d2 取 c1 高 nibble + c2
    d1[t] = c0 + 256 * (c1 & 0x0F)
    d2[t] = (c1 >> 4) + 16 * c2
    pos += 3
  return d1, d2


def rej_scalar_from_d12(d1: np.ndarray, d2: np.ndarray, q: int = KYBER_Q, n: int = KYBER_N) -> np.ndarray:
  """规范顺序 rejection sampling：按 t=0..223 扫描，每对先 d1[t] 后 d2[t]，接受 v<q 直至满 n。

  这是 FIPS/Kyber 文义上的「边扫边填」语义，也是 f203_alg7_rej_scalar 的对照 golden。
  满 n=256 即停止，**不**继续消费后续候选（与批量路径截取前 n 个接受项一致）。
  """
  out: list[int] = []
  for i in range(d1.shape[0]):
    v1 = int(d1[i])
    if v1 < q and len(out) < n:
      out.append(v1)
    v2 = int(d2[i])
    if v2 < q and len(out) < n:
      out.append(v2)
  if len(out) < n:
    # 672B 固定预 squeeze 在统计上应几乎总能凑满 256；若触发说明几何常量或 XOF 长度有误
    raise SystemExit(
        f"rej: only {len(out)} coeffs from {XOF_BYTES}B xof "
        f"(672B 固定预 squeeze 应极少触发；检查几何常量)"
    )
  return np.array(out[:n], dtype=np.int32)


def rej_bulk_from_d12(d1: np.ndarray, d2: np.ndarray, q: int = KYBER_Q, n: int = KYBER_N) -> np.ndarray:
  """批量路径 golden：先对全 448 lane 做 v>=q → q 标记，再取所有 v<q 的前 n 个。

  用于验证向量 rej（全量 stream + 掩码过滤 + compact）与规范顺序语义等价。
  向量实现通常走此路径的数学等价形式，而非字面 for 循环早停。
  """
  stream: list[int] = []
  for i in range(d1.shape[0]):
    v1 = int(d1[i])
    # 拒绝项标记为 q（哨兵），后续 filter 时 v<q 才保留
    stream.append(v1 if v1 < q else q)
    v2 = int(d2[i])
    stream.append(v2 if v2 < q else q)
  out = [x for x in stream if x < q]
  if len(out) < n:
    raise SystemExit(f"bulk rej: only {len(out)} coeffs from {XOF_BYTES}B stream")
  return np.array(out[:n], dtype=np.int32)


def main() -> None:
  # 与 run.sh 导出、kernel host 侧读取的环境变量对齐
  seed_d = int(os.environ.get("SEED_D", str(SEED_D_DEFAULT)))
  poly_j = int(os.environ.get("ALG7_POLY_J", "0"))
  poly_i = int(os.environ.get("ALG7_POLY_I", "0"))

  # --- Alg.7 SampleNTT 主链 ---
  d = derand_bytes_from_seed(seed_d)
  rho = hash_g_rho(d)
  seed = rho + bytes([poly_j & 0xFF, poly_i & 0xFF])
  xof = np.frombuffer(shake128_squeeze(seed, XOF_BYTES), dtype=np.uint8)
  d1, d2 = unpack_d12_from_xof(xof.tobytes())
  a_hat_spec = rej_scalar_from_d12(d1, d2)
  a_hat_bulk = rej_bulk_from_d12(d1, d2)
  # 双路径一致性：向量 rej 必须与规范顺序 golden 比特级一致
  if not np.array_equal(a_hat_spec, a_hat_bulk):
    raise SystemExit("spec vs bulk golden mismatch")

  input_dir = ROOT / "input"
  output_dir = ROOT / "output"
  input_dir.mkdir(exist_ok=True)
  output_dir.mkdir(exist_ok=True)

  # host 输入：kernel 据此重现 ρ 与 sample_seed（poly 坐标由 poly_ij.bin 传入）
  (input_dir / "seed_d.bin").write_bytes(struct.pack("<I", seed_d))
  (input_dir / "poly_ij.bin").write_bytes(bytes([poly_j & 0xFF, poly_i & 0xFF]))
  # golden 期望（verify_result.py 对拍 kernel 写入的 output/*.bin）
  xof.tofile(output_dir / "golden_xof.bin")
  d1.tofile(output_dir / "golden_d1.bin")
  d2.tofile(output_dir / "golden_d2.bin")
  a_hat_spec.tofile(output_dir / "golden_a_hat.bin")

  print(f"[gen_data] SEED_D={seed_d} j={poly_j} i={poly_i} xof_bytes={XOF_BYTES}")
  print(f"[gen_data] golden_xof {xof.shape} golden_d1 {d1.shape} golden_d2 {d2.shape} golden_a_hat {a_hat_spec.shape}")


if __name__ == "__main__":
  main()
