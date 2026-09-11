#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RB-T10 / T08 拓扑数学积木（Host golden / 设备对拍 oracle 共用）。

流水线位置：Encrypt 域拼装 G3 的 NTT 域乘 + INTT + 时域加噪。
对齐 FIPS 203：Alg.11 MultiplyNTTs、Alg.10 InverseNTT、Alg.14 的
  u = INTT(Âᵀ∘ŷ)+e₁ ， v = INTT(⟨t̂,ŷ⟩)+e₂+μ 。

来源约定：
  - ζ / γ 表只读 thirdparty/ntt_onnx/.../mlkem_ntt_tables.h（FIPS Appendix A）
  - **禁止** 抄 alg14 / Encrypt / Encaps / Decaps / frozen 实现源码
  - 本文件仅为 graph-tests Host golden oracle，不是 AscendC 设备规格
  - T10 设备侧在 uv_device_math.hpp 按同公式自研实现（禁 Host 预喂最终 u,v）
"""
from __future__ import annotations

import re
from pathlib import Path
from typing import List, Sequence

import numpy as np

Q = 3329
N = 256
K = 4
# FIPS 203 Alg.10：乘以 128^{-1} mod q
INTT_SCALE = 3303

_SCRIPT = Path(__file__).resolve().parent


def _ascendc_repo_root(start: Path) -> Path:
    """自 start 向上查找含 AGENTS.md 与 scripts/ 的仓库根。"""
    p = start.resolve()
    for d in [p, *p.parents]:
        if (d / "AGENTS.md").is_file() and (d / "scripts").is_dir():
            return d
    raise RuntimeError(f"cannot locate ascendc repo root from {start}")


_REPO = _ascendc_repo_root(_SCRIPT)
_TABLES_H = _REPO / "thirdparty/ntt_onnx/include/mlkem/stable/mlkem_ntt_tables.h"


def _parse_i16_array(text: str, symbol: str, expect: int) -> List[int]:
    """从 C 头解析 int16 数组字面量。"""
    m = re.search(rf"{symbol}\s*\[[^\]]*\]\s*=\s*\{{(.*?)\}};", text, re.S)
    if not m:
        raise ValueError(f"Cannot locate {symbol} in {_TABLES_H}")
    nums = [int(x) for x in re.findall(r"-?\d+", m.group(1))]
    if len(nums) != expect:
        raise ValueError(f"{symbol} size {len(nums)} != {expect}")
    return nums


def load_zetas_gammas() -> tuple[List[int], List[int]]:
    """加载 kMlkemZetas[128]、kMlkemGammas[128]。"""
    text = _TABLES_H.read_text(encoding="utf-8")
    return _parse_i16_array(text, "kMlkemZetas", 128), _parse_i16_array(text, "kMlkemGammas", 128)


_ZETAS, _GAMMAS = load_zetas_gammas()


def mod_q(x: int) -> int:
    """规范到 [0, q)。"""
    r = int(x) % Q
    return r + Q if r < 0 else r


def mlkem_ntt(coeffs: Sequence[int]) -> List[int]:
    """
    FIPS 203 Alg.9 正向 NTT（标量蝶形；供预生成 ŷ/Â 自洽时可选）。
    @param coeffs 长度 256 时域系数
    @return NTT 域系数列表
    """
    f = [mod_q(c) for c in coeffs]
    i = 1
    for length in (128, 64, 32, 16, 8, 4, 2):
        for start in range(0, N, 2 * length):
            zeta = _ZETAS[i]
            i += 1
            for j in range(start, start + length):
                t = mod_q(zeta * f[j + length])
                f[j + length] = mod_q(f[j] - t)
                f[j] = mod_q(f[j] + t)
    return f


def mlkem_intt(coeffs: Sequence[int]) -> List[int]:
    """
    FIPS 203 Alg.10 InverseNTT。
    背景：表 kMlkemZetas[i]=ζ^{BitRev7(i)}；Alg.10 用 ζ^{−BitRev7(i)}≡−zetas[i] (mod q)
    （因 ζ^{128}≡−1，与本仓 plain-mul 标量路径自洽；已对 roundtrip / negacyclic 乘校过）。
    @param coeffs NTT 域长度 256
    @return 时域 [0,q)
    """
    f = [mod_q(c) for c in coeffs]
    i = 127
    for length in (2, 4, 8, 16, 32, 64, 128):
        for start in range(0, N, 2 * length):
            zeta = mod_q(-_ZETAS[i])
            i -= 1
            for j in range(start, start + length):
                t = f[j]
                f[j] = mod_q(t + f[j + length])
                f[j + length] = mod_q(zeta * (t - f[j + length]))
    return [mod_q(x * INTT_SCALE) for x in f]


def multiply_ntts(f: Sequence[int], g: Sequence[int]) -> List[int]:
    """
    FIPS 203 Alg.11 MultiplyNTTs：偶奇对 + γ·BaseCaseMultiply。
    语义对齐活跃探针 hat_inner_product_ref 的乘对公式（仅数学契约，非抄设备码）。
    """
    h = [0] * N
    for i in range(N // 2):
        a0, a1 = int(f[2 * i]), int(f[2 * i + 1])
        b0, b1 = int(g[2 * i]), int(g[2 * i + 1])
        gamma = _GAMMAS[i]
        a1b1 = mod_q(a1 * b1)
        h[2 * i] = mod_q(a0 * b0 + a1b1 * gamma)
        h[2 * i + 1] = mod_q(a0 * b1 + a1 * b0)
    return h


def matrix_transpose_mul_yhat(a_hat: np.ndarray, y_hat: np.ndarray) -> np.ndarray:
    """
    计算 û = Âᵀ ∘ ŷ ∈ (Z_q[X]/(X^n+1))^k（NTT 域）。

    布局：a_hat 行主序 [K,K,N]，flat(p,j,*)=(p*K+j)*N；
      û[i] = Σ_j MultiplyNTTs(Â[j,i], ŷ[j])   （因 Âᵀ[i,j]=Â[j,i]）
    lazy int64 累加后一次 mod q（K=4 足够小）。
    """
    assert a_hat.shape == (K, K, N)
    assert y_hat.shape == (K, N)
    out = np.zeros((K, N), dtype=np.int32)
    for i in range(K):
        acc = np.zeros(N, dtype=np.int64)
        for j in range(K):
            prod = multiply_ntts(a_hat[j, i].tolist(), y_hat[j].tolist())
            acc += np.asarray(prod, dtype=np.int64)
        out[i] = np.array([mod_q(int(x)) for x in acc], dtype=np.int32)
    return out


def inner_product_that_yhat(t_hat: np.ndarray, y_hat: np.ndarray) -> np.ndarray:
    """
    计算 v̂ = ⟨t̂, ŷ⟩ = Σ_{i=0}^{k-1} MultiplyNTTs(t̂[i], ŷ[i])（NTT 域单 poly）。
    """
    assert t_hat.shape == (K, N)
    assert y_hat.shape == (K, N)
    acc = np.zeros(N, dtype=np.int64)
    for i in range(K):
        prod = multiply_ntts(t_hat[i].tolist(), y_hat[i].tolist())
        acc += np.asarray(prod, dtype=np.int64)
    return np.array([mod_q(int(x)) for x in acc], dtype=np.int32)


def mu_embed_from_m(m: bytes) -> np.ndarray:
    """
    T04 逻辑：ByteDecode₁ LSB + Decompress₁ → μ[256]。
    μ_i = (bit_i * q + 1) >> 1 ∈ {0, 1665}。
    """
    assert len(m) == 32
    out = np.zeros(N, dtype=np.int32)
    for i in range(N):
        bit = (m[i // 8] >> (i % 8)) & 1
        out[i] = (bit * Q + 1) >> 1
    return out


def compute_u_v(
    a_hat: np.ndarray,
    y_hat: np.ndarray,
    t_hat: np.ndarray,
    e1: np.ndarray,
    e2: np.ndarray,
    mu: np.ndarray,
) -> tuple[np.ndarray, np.ndarray]:
    """
    G3 拓扑拼装（Host 孪生入口）：
      u = INTT(Âᵀ∘ŷ) + e₁
      v = INTT(⟨t̂,ŷ⟩) + e₂ + μ
    加噪后系数再 mod q。
    @return u int32[K,N], v int32[N]
    """
    u_hat = matrix_transpose_mul_yhat(a_hat, y_hat)
    v_hat = inner_product_that_yhat(t_hat, y_hat)

    u = np.zeros((K, N), dtype=np.int32)
    for i in range(K):
        time_u = mlkem_intt(u_hat[i].tolist())
        for c in range(N):
            u[i, c] = mod_q(time_u[c] + int(e1[i, c]))

    time_v = mlkem_intt(v_hat.tolist())
    v = np.zeros(N, dtype=np.int32)
    for c in range(N):
        v[c] = mod_q(time_v[c] + int(e2[c]) + int(mu[c]))
    return u, v
