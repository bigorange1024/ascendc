#!/usr/bin/env python3
# coding=utf-8
"""
@file gen_data.py
@brief exp-fips203-mlkem-pke-encrypt-k2 — 全链 Encrypt golden 生成（自包含）。

流水线（INTEGRATION_PLAN §4.1、§8）：
  * 锁死 SEED_D=20260619；本地生成 ML-KEM-512 ek/m/coins/golden_c
  * 本地派生 LUT 与 golden_v（CPU 分段注入；SIM 全设备不需 v）
  * Alg.14 产物仅 c（golden/c.bin）；u/v 为中间量
输出：input/{ek_pke,m,coins,lut_*,golden_v}.bin、golden/c.bin
"""
from __future__ import annotations

import os
import subprocess
import sys

import numpy as np

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_CASE_DIR = os.path.normpath(os.path.join(_SCRIPT_DIR, ".."))
_HOST_GOLDEN = os.path.join(_SCRIPT_DIR, "host_golden")
sys.path.insert(0, _HOST_GOLDEN)

from f203_ref_common import (  # noqa: E402
    K,
    N,
    Q,
    embed_message,
    load_lut_t_i8,
    stage123_transform,
)
import golden_c as gc  # noqa: E402

SEED_D = 20260619
_LOCKED_INPUTS = ("ek_pke.bin", "m.bin", "coins.bin")
_EK_BYTES = 800
_MSG_BYTES = 32
_COINS_BYTES = 32


def _lut_planar_stacked(lut: np.ndarray, even: bool) -> np.ndarray:
    """把 [256,512] int8 LUT 折成 [512,128] planar-stacked（top‖bottom），与 compute kernel 约定一致。"""
    if even:
        top = lut[:, 0:N:2]
        bottom = lut[:, N:512:2]
    else:
        top = lut[:, 1:N:2]
        bottom = lut[:, N + 1 : 512 : 2]
    return np.concatenate([top, bottom], axis=0)


def _gen_lut_bins(inp: str) -> None:
    """写 NTT/INTT even/odd stacked LUT 到 input/。"""
    lut_ntt = load_lut_t_i8("ntt")
    lut_intt = load_lut_t_i8("intt")
    _lut_planar_stacked(lut_ntt, True).tofile(os.path.join(inp, "lut_ntt_even_stacked.bin"))
    _lut_planar_stacked(lut_ntt, False).tofile(os.path.join(inp, "lut_ntt_odd_stacked.bin"))
    _lut_planar_stacked(lut_intt, True).tofile(os.path.join(inp, "lut_intt_even_stacked.bin"))
    _lut_planar_stacked(lut_intt, False).tofile(os.path.join(inp, "lut_intt_odd_stacked.bin"))


def _gen_locked_inputs_local(inp: str) -> None:
    """本目录自生成 k2 ek/m/coins（与参数卡 derand 前缀一致）。"""
    ek_path = os.path.join(inp, "ek_pke.bin")
    subprocess.check_call(
        [sys.executable, os.path.join(_HOST_GOLDEN, "gen_ek_pke.py"), str(SEED_D), ek_path],
        cwd=_CASE_DIR,
    )
    rng = np.random.default_rng(SEED_D + 991)
    m = rng.integers(0, 256, size=_MSG_BYTES, dtype=np.uint8)
    coins = rng.integers(0, 256, size=_COINS_BYTES, dtype=np.uint8)
    m.tofile(os.path.join(inp, "m.bin"))
    coins.tofile(os.path.join(inp, "coins.bin"))


def _ensure_locked_inputs(inp: str) -> str:
    """生成锁定输入并返回来源标签。"""
    _gen_locked_inputs_local(inp)
    return "local"


def _write_golden_c(gold: str, ek: bytes, m: bytes, coins: bytes, prefer_corr: bool) -> tuple[bytes, str]:
    """写 golden/c.bin；prefer_corr 保留签名兼容，k2 始终本地重算。"""
    dst = os.path.join(gold, "c.bin")
    _ = prefer_corr
    c_ref = bytes(gc.golden_encrypt(ek, m, coins))
    with open(dst, "wb") as f:
        f.write(c_ref)
    return c_ref, "local"


def _compute_golden_v(ek: bytes, m: bytes, coins: bytes) -> np.ndarray:
    """v = INTT(Σ_j t̂[j]∘r̂[j]) + e₂ + μ(m)（与设备 e₂+=μ 折叠、k2 polyvec4 INTT 结果等价）。"""
    t_hat = gc.decode_t_hat(ek[: gc.EK_T_BYTES])
    r, _e1, e2 = gc.build_re(coins)
    r_hat = stage123_transform(r, "ntt")
    tr_hat = gc.golden_tr_hat(t_hat, r_hat)
    tr_pad = np.zeros((K, N), dtype=np.int32)
    tr_pad[0] = tr_hat
    tr = stage123_transform(tr_pad, "intt")[0]
    v = embed_message(tr, m)  # + μ(m)
    v = ((v.astype(np.int64) + e2.astype(np.int64)) % Q).astype(np.int32)
    return v


def main() -> None:
    """全链：锁定输入 → LUT → golden_c → golden_v（CPU 注入）。"""
    inp = os.path.join(_CASE_DIR, "input")
    out = os.path.join(_CASE_DIR, "output")
    gold = os.path.join(_CASE_DIR, "golden")
    for d in (inp, out, gold):
        os.makedirs(d, exist_ok=True)

    # —— ek/m/coins ——
    src_in = _ensure_locked_inputs(inp)
    _gen_lut_bins(inp)

    ek = open(os.path.join(inp, "ek_pke.bin"), "rb").read()
    m = open(os.path.join(inp, "m.bin"), "rb").read()
    coins = open(os.path.join(inp, "coins.bin"), "rb").read()
    if len(ek) != _EK_BYTES or len(m) != _MSG_BYTES or len(coins) != _COINS_BYTES:
        raise SystemExit(f"[gen_data] bad sizes ek={len(ek)} m={len(m)} coins={len(coins)}")

    # —— c 与 CPU 用 v ——
    _c_bytes, src_c = _write_golden_c(gold, ek, m, coins, prefer_corr=(src_in == "correctness"))

    v = _compute_golden_v(ek, m, coins)
    v.tofile(os.path.join(inp, "golden_v.bin"))

    print(
        f"[gen_data] SEED_D={SEED_D} input={src_in} golden_c={src_c}；"
        f"c={len(_c_bytes)}B；golden_v(CPU 注入) OK"
    )


if __name__ == "__main__":
    main()
