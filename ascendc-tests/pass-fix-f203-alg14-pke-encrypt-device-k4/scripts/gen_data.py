#!/usr/bin/env python3
# coding=utf-8
"""
pass-fix-f203-alg14-pke-encrypt-device-k4 — 全链 Encrypt golden 生成。

设计（见 INTEGRATION_PLAN §4.1、§8）：
  * 锁死 SEED_D=20260619，全链唯一种子。
  * **复用** correctness 探针 fix-f203-alg14-pke-encrypt-correctness-k4 的现成产物：
      - input/ek_pke.bin、m.bin、coins.bin  ← 锁定输入（复制）
      - output/golden_c.bin                 ← 期望密文（复制到本探针 golden/c.bin）
  * 本地派生（确定性，非随机）：
      - LUT：lut_ntt_even/odd、lut_intt_even/odd（供 compute kernel NTT/INTT）
      - input/golden_v.bin：v = INTT(t̂·r̂) + e₂ + μ(m)。**仅 CPU 分段实现的注入数据**
        （CPU 三 launch 无 k=8 INTT 不产 v），非 Alg.14 输出；SIM 全设备不需要它。
  * 三源一致性自检：本地用 correctness host_golden 参考重算 c，须与复制的 golden_c 逐字节相等。

Alg.14 输出只有密文 c（golden/c.bin）；u/v 为内部中间量，不作为产物。
输出：input/{ek_pke,m,coins,lut_*,golden_v}.bin、golden/c.bin
"""
from __future__ import annotations

import os
import shutil
import sys

import numpy as np

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_CASE_DIR = os.path.normpath(os.path.join(_SCRIPT_DIR, ".."))
_HOST_GOLDEN = os.path.join(_SCRIPT_DIR, "host_golden")
sys.path.insert(0, _HOST_GOLDEN)

# host_golden 参考（自 correctness 探针复制；LUT 路径已改指仓库级 ntt_study）
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
_CORR = os.path.normpath(os.path.join(_CASE_DIR, "..", "fix-f203-alg14-pke-encrypt-correctness-k4"))
_LOCKED_INPUTS = ("ek_pke.bin", "m.bin", "coins.bin")


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
    lut_ntt = load_lut_t_i8("ntt")
    lut_intt = load_lut_t_i8("intt")
    _lut_planar_stacked(lut_ntt, True).tofile(os.path.join(inp, "lut_ntt_even_stacked.bin"))
    _lut_planar_stacked(lut_ntt, False).tofile(os.path.join(inp, "lut_ntt_odd_stacked.bin"))
    _lut_planar_stacked(lut_intt, True).tofile(os.path.join(inp, "lut_intt_even_stacked.bin"))
    _lut_planar_stacked(lut_intt, False).tofile(os.path.join(inp, "lut_intt_odd_stacked.bin"))


def _copy_locked_inputs(inp: str) -> None:
    corr_in = os.path.join(_CORR, "input")
    for name in _LOCKED_INPUTS:
        src = os.path.join(corr_in, name)
        if not os.path.isfile(src):
            raise SystemExit(
                f"[gen_data] 缺少 correctness 锁定输入 {src}；"
                f"请先在 {_CORR} 跑一次 gen_data（默认 SEED_D={SEED_D}）"
            )
        shutil.copyfile(src, os.path.join(inp, name))


def _copy_golden_c(gold: str) -> bytes:
    src = os.path.join(_CORR, "output", "golden_c.bin")
    if not os.path.isfile(src):
        raise SystemExit(f"[gen_data] 缺少 correctness golden {src}（需 ENCRYPT_VERIFY=1 生成）")
    dst = os.path.join(gold, "c.bin")
    shutil.copyfile(src, dst)
    with open(dst, "rb") as f:
        return f.read()


def _compute_golden_v(ek: bytes, m: bytes, coins: bytes) -> np.ndarray:
    """v = INTT(Σ_j t̂[j]∘r̂[j]) + e₂ + μ(m)（与设备 e₂+=μ 折叠、K=4 INTT 结果等价）。"""
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
    inp = os.path.join(_CASE_DIR, "input")
    out = os.path.join(_CASE_DIR, "output")
    gold = os.path.join(_CASE_DIR, "golden")
    for d in (inp, out, gold):
        os.makedirs(d, exist_ok=True)

    # 1. 复制锁定输入 + 期望密文
    _copy_locked_inputs(inp)
    golden_c_bytes = _copy_golden_c(gold)

    # 2. 本地 LUT（确定性）
    _gen_lut_bins(inp)

    ek = open(os.path.join(inp, "ek_pke.bin"), "rb").read()
    m = open(os.path.join(inp, "m.bin"), "rb").read()
    coins = open(os.path.join(inp, "coins.bin"), "rb").read()

    # 3. 三源一致性自检：本地参考重算 c 须与复制的 golden_c 相等
    c_ref = gc.golden_encrypt(ek, m, coins)
    if bytes(c_ref) != golden_c_bytes:
        raise SystemExit("[gen_data] 一致性失败：本地重算 c != correctness golden_c.bin")

    # 4. CPU 分段注入用 golden_v（放 input/，非 Alg.14 产物）
    v = _compute_golden_v(ek, m, coins)
    v.tofile(os.path.join(inp, "golden_v.bin"))

    print(f"[gen_data] SEED_D={SEED_D} 复用 correctness 输入/golden_c；c 一致；golden_v(CPU 注入) OK")


if __name__ == "__main__":
    main()
