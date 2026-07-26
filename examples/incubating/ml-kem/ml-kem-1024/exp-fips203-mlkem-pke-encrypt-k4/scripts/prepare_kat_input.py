#!/usr/bin/env python3
# coding=utf-8
"""
prepare_kat_input.py — 把 liboqs KAT fixture 写成本用例 input/golden。

流水线位置：FIPS 203 Alg.14 / ML-KEM-1024 Encrypt 的 KAT 批测胶水。
写出 ek/m/coins、LUT、golden_v、golden/c，供 ENCRYPT_KAT=1 跑 run.sh。
与 golden：c 来自 liboqs，用于交叉验证设备 output/c.bin。
"""
from __future__ import annotations

import argparse
import os
import shutil
import sys
from pathlib import Path

import numpy as np

_SCRIPT_DIR = Path(__file__).resolve().parent
_CASE_DIR = _SCRIPT_DIR.parent
_HOST_GOLDEN = _SCRIPT_DIR / "host_golden"
sys.path.insert(0, str(_HOST_GOLDEN))

from f203_ref_common import (  # noqa: E402
    K,
    N,
    Q,
    embed_message,
    load_lut_t_i8,
    stage123_transform,
)
import golden_c as gc  # noqa: E402

EK_BYTES = 1568
MSG_BYTES = 32
COINS_BYTES = 32
CT_BYTES = 1568


def _lut_planar_stacked(lut: np.ndarray, even: bool) -> np.ndarray:
    """[256,512] int8 → [512,128] planar-stacked（top‖bottom）。"""
    if even:
        top = lut[:, 0:N:2]
        bottom = lut[:, N:512:2]
    else:
        top = lut[:, 1:N:2]
        bottom = lut[:, N + 1 : 512 : 2]
    return np.concatenate([top, bottom], axis=0)


def _gen_lut_bins(inp: Path) -> None:
    lut_ntt = load_lut_t_i8("ntt")
    lut_intt = load_lut_t_i8("intt")
    _lut_planar_stacked(lut_ntt, True).tofile(inp / "lut_ntt_even_stacked.bin")
    _lut_planar_stacked(lut_ntt, False).tofile(inp / "lut_ntt_odd_stacked.bin")
    _lut_planar_stacked(lut_intt, True).tofile(inp / "lut_intt_even_stacked.bin")
    _lut_planar_stacked(lut_intt, False).tofile(inp / "lut_intt_odd_stacked.bin")


def _compute_golden_v(ek: bytes, m: bytes, coins: bytes) -> np.ndarray:
    """v = INTT(t̂·r̂) + e₂ + μ(m)；仅 CPU 注入。"""
    t_hat = gc.decode_t_hat(ek[: gc.EK_T_BYTES])
    r, _e1, e2 = gc.build_re(coins)
    r_hat = stage123_transform(r, "ntt")
    tr_hat = gc.golden_tr_hat(t_hat, r_hat)
    tr_pad = np.zeros((K, N), dtype=np.int32)
    tr_pad[0] = tr_hat
    tr = stage123_transform(tr_pad, "intt")[0]
    v = embed_message(tr, m)
    v = ((v.astype(np.int64) + e2.astype(np.int64)) % Q).astype(np.int32)
    return v


def prepare(
    ek_path: Path,
    m_path: Path,
    coins_path: Path,
    c_ref_path: Path | None = None,
    case_dir: Path | None = None,
) -> None:
    """
    写入 case_dir/input 与 golden/c.bin。

    @param ek_path / m_path / coins_path 外部字节源（liboqs 或 KeyGen）
    @param c_ref_path 若给：复制为 golden/c.bin 并用 host 重算自检；否则 host golden_encrypt
    """
    root = (case_dir or _CASE_DIR).resolve()
    inp = root / "input"
    out = root / "output"
    gold = root / "golden"
    for d in (inp, out, gold):
        d.mkdir(parents=True, exist_ok=True)

    ek = ek_path.read_bytes()
    m = m_path.read_bytes()
    coins = coins_path.read_bytes()
    if len(ek) != EK_BYTES or len(m) != MSG_BYTES or len(coins) != COINS_BYTES:
        raise SystemExit(
            f"[prepare_kat] bad sizes ek={len(ek)} m={len(m)} coins={len(coins)}"
        )

    shutil.copy2(ek_path, inp / "ek_pke.bin")
    shutil.copy2(m_path, inp / "m.bin")
    shutil.copy2(coins_path, inp / "coins.bin")
    _gen_lut_bins(inp)

    c_host = bytes(gc.golden_encrypt(ek, m, coins))
    if len(c_host) != CT_BYTES:
        raise SystemExit(f"[prepare_kat] host c size {len(c_host)}")
    if c_ref_path is not None:
        c_ref = c_ref_path.read_bytes()
        if c_ref != c_host:
            raise SystemExit("[prepare_kat] host golden_encrypt != 外部 c 参考（liboqs/fixture）")
        (gold / "c.bin").write_bytes(c_ref)
        src = "external"
    else:
        (gold / "c.bin").write_bytes(c_host)
        src = "host"

    _compute_golden_v(ek, m, coins).tofile(inp / "golden_v.bin")
    print(f"[prepare_kat] OK golden_c={src} ek/m/coins+lut+golden_v → {inp}")


def main() -> None:
    ap = argparse.ArgumentParser(description="exp-encrypt KAT/roundtrip input 准备")
    ap.add_argument("--ek", type=Path, required=True)
    ap.add_argument("--m", type=Path, required=True)
    ap.add_argument("--coins", type=Path, required=True)
    ap.add_argument("--c-ref", type=Path, default=None, help="可选：liboqs c.bin 作 golden 并自检")
    ap.add_argument("--case-dir", type=Path, default=_CASE_DIR)
    args = ap.parse_args()
    prepare(
        args.ek.resolve(),
        args.m.resolve(),
        args.coins.resolve(),
        args.c_ref.resolve() if args.c_ref else None,
        args.case_dir.resolve(),
    )


if __name__ == "__main__":
    main()
