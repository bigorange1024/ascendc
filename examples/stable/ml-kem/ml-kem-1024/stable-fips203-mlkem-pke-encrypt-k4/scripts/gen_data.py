#!/usr/bin/env python3
# coding=utf-8
"""
gen_data.py — stable-fips203-mlkem-pke-encrypt-k4 全链 Encrypt golden 生成（自包含）。

流水线位置：FIPS 203 Alg.14 / ML-KEM-1024 K-PKE.Encrypt 的 **host 数据准备**；
`run.sh` 调用本脚本写出 `input/` 与 `golden/c.bin`，供设备 `output/c.bin` 对拍。
Alg.14 输出只有密文 c；Â/y/u/v 等为设备内部中间量，禁止作为产物落盘。

设计：
  * SEED_D：环境变量定点；未设则 SHA3 派生（library/shared/fips203_host_rng）。
  * m/coins：由 SEED_D 经 SHAKE256 域分离扩字节（不再 numpy default_rng）。
  * 仅当显式定点 SEED_D=20260619 且历史 correctness 输入存在时，才复用冻结 fixture。
  * 本地派生 LUT 与 CPU 专用 golden_v（仅 CPU 分段注入）。
"""
from __future__ import annotations

from pathlib import Path

import os

def _ascendc_repo_root(start: Path) -> Path:
    """自 start 向上查找含 AGENTS.md 与 scripts/ 的仓库根（兼容 ml-kem 参数组嵌套）。"""
    p = start.resolve() if isinstance(start, Path) else Path(start).resolve()
    for d in [p, *p.parents]:
        if (d / "AGENTS.md").is_file() and (d / "scripts").is_dir():
            return d
    raise RuntimeError(f"cannot locate ascendc repo root from {start}")

import shutil
import subprocess
import sys

import numpy as np

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_CASE_DIR = os.path.normpath(os.path.join(_SCRIPT_DIR, ".."))
_HOST_GOLDEN = os.path.join(_SCRIPT_DIR, "host_golden")
sys.path.insert(0, _HOST_GOLDEN)
_REPO = str(_ascendc_repo_root(Path(_CASE_DIR)))
sys.path.insert(0, os.path.join(_REPO, "library/shared/fips203_host_rng"))

from f203_ref_common import (  # noqa: E402
    K,
    N,
    Q,
    embed_message,
    load_lut_t_i8,
    stage123_transform,
)
import golden_c as gc  # noqa: E402
from host_rng import expand_bytes, resolve_seed_d  # noqa: E402

_CASE_TAG = "fips203-mlkem-pke-encrypt-k4"
# 可选对照目录（历史 correctness 已冻结；仅定点 SEED_D=20260619 时可选复用）
_CORR = os.path.normpath(
    os.path.join(
        _CASE_DIR,
        "..",
        "..",
        "..",
        "ascendc-tests",
        "frozen",
        "frozen-fix-f203-alg14-pke-encrypt-correctness-k4",
    )
)
_LOCKED_INPUTS = ("ek_pke.bin", "m.bin", "coins.bin")
_EK_BYTES = 1568
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
    lut_ntt = load_lut_t_i8("ntt")
    lut_intt = load_lut_t_i8("intt")
    _lut_planar_stacked(lut_ntt, True).tofile(os.path.join(inp, "lut_ntt_even_stacked.bin"))
    _lut_planar_stacked(lut_ntt, False).tofile(os.path.join(inp, "lut_ntt_odd_stacked.bin"))
    _lut_planar_stacked(lut_intt, True).tofile(os.path.join(inp, "lut_intt_even_stacked.bin"))
    _lut_planar_stacked(lut_intt, False).tofile(os.path.join(inp, "lut_intt_odd_stacked.bin"))


def _corr_inputs_ready() -> bool:
    corr_in = os.path.join(_CORR, "input")
    return all(os.path.isfile(os.path.join(corr_in, name)) for name in _LOCKED_INPUTS)


def _corr_golden_ready() -> bool:
    return os.path.isfile(os.path.join(_CORR, "output", "golden_c.bin"))


def _copy_locked_inputs(inp: str) -> None:
    corr_in = os.path.join(_CORR, "input")
    for name in _LOCKED_INPUTS:
        shutil.copyfile(os.path.join(corr_in, name), os.path.join(inp, name))


def _gen_locked_inputs_local(inp: str, seed_d: int) -> None:
    """本目录自生成 ek/m/coins：ek 由 seed_d KeyGen；m/coins 为 SHAKE256 扩字节。"""
    ek_path = os.path.join(inp, "ek_pke.bin")
    subprocess.check_call(
        [sys.executable, os.path.join(_HOST_GOLDEN, "gen_ek_pke.py"), str(seed_d), ek_path],
        cwd=_CASE_DIR,
    )
    m = np.frombuffer(expand_bytes(_CASE_TAG, "m", seed_d, _MSG_BYTES), dtype=np.uint8).copy()
    coins = np.frombuffer(expand_bytes(_CASE_TAG, "coins", seed_d, _COINS_BYTES), dtype=np.uint8).copy()
    m.tofile(os.path.join(inp, "m.bin"))
    coins.tofile(os.path.join(inp, "coins.bin"))


def _ensure_locked_inputs(inp: str, seed_d: int, how: str) -> str:
    """返回来源标签：'correctness' | 'local'。"""
    if how == "env" and seed_d == 20260619 and _corr_inputs_ready():
        _copy_locked_inputs(inp)
        return "correctness"
    print(f"[gen_data] 本地生成 SEED_D={seed_d} via={how}（ek + SHAKE m/coins）")
    _gen_locked_inputs_local(inp, seed_d)
    return "local"


def _write_golden_c(gold: str, ek: bytes, m: bytes, coins: bytes, prefer_corr: bool) -> tuple[bytes, str]:
    """写 golden/c.bin；返回 (bytes, 来源标签)。"""
    dst = os.path.join(gold, "c.bin")
    if prefer_corr and _corr_golden_ready():
        src = os.path.join(_CORR, "output", "golden_c.bin")
        shutil.copyfile(src, dst)
        with open(dst, "rb") as f:
            golden_c_bytes = f.read()
        c_ref = bytes(gc.golden_encrypt(ek, m, coins))
        if c_ref != golden_c_bytes:
            raise SystemExit("[gen_data] 一致性失败：本地重算 c != correctness golden_c.bin")
        return golden_c_bytes, "correctness"
    c_ref = bytes(gc.golden_encrypt(ek, m, coins))
    with open(dst, "wb") as f:
        f.write(c_ref)
    return c_ref, "local"


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
    seed_d, how = resolve_seed_d(_CASE_TAG)
    inp = os.path.join(_CASE_DIR, "input")
    out = os.path.join(_CASE_DIR, "output")
    gold = os.path.join(_CASE_DIR, "golden")
    for d in (inp, out, gold):
        os.makedirs(d, exist_ok=True)

    src_in = _ensure_locked_inputs(inp, seed_d, how)
    _gen_lut_bins(inp)

    ek = open(os.path.join(inp, "ek_pke.bin"), "rb").read()
    m = open(os.path.join(inp, "m.bin"), "rb").read()
    coins = open(os.path.join(inp, "coins.bin"), "rb").read()
    if len(ek) != _EK_BYTES or len(m) != _MSG_BYTES or len(coins) != _COINS_BYTES:
        raise SystemExit(f"[gen_data] bad sizes ek={len(ek)} m={len(m)} coins={len(coins)}")

    _c_bytes, src_c = _write_golden_c(gold, ek, m, coins, prefer_corr=(src_in == "correctness"))

    v = _compute_golden_v(ek, m, coins)
    v.tofile(os.path.join(inp, "golden_v.bin"))

    print(
        f"[gen_data] SEED_D={seed_d} via={how} input={src_in} golden_c={src_c}；"
        f"c={len(_c_bytes)}B；golden_v(CPU 注入) OK"
    )


if __name__ == "__main__":
    main()
