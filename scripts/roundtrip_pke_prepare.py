#!/usr/bin/env python3
"""
roundtrip_pke_prepare.py — 为 device Encrypt→Decrypt 闭环准备 input/。

- ek/dk 来自 KeyGen 探针 output/（device 产出，非 host golden gen_ek/gen_dk）。
- m/coins 与两探针 gen_data 同规则（SEED_D + 991）。
- 不生成 host golden_c / golden_m（闭环验 m_ref，非 oracle）。
"""
from __future__ import annotations

import argparse
import os
import shutil
import struct
import subprocess
import sys
from pathlib import Path

import numpy as np

EK_BYTES = 1568
DK_BYTES = 1536
CT_BYTES = 1568
MSG_BYTES = 32
COINS_BYTES = 32
SEED_D_DEFAULT = 20260619


def _write_m_coins(inp: Path, seed_d: int) -> None:
    rng = np.random.default_rng(seed_d + 991)
    m = rng.integers(0, 256, size=MSG_BYTES, dtype=np.uint8)
    coins = rng.integers(0, 256, size=COINS_BYTES, dtype=np.uint8)
    m.tofile(inp / "m.bin")
    coins.tofile(inp / "coins.bin")


def _run_ntt_lut_bins(case_dir: Path, inp: Path) -> None:
    lut_py = case_dir / "scripts" / "host_golden" / "ntt_lut_bins.py"
    if not lut_py.is_file():
        print(f"[roundtrip_prepare] missing {lut_py}", file=sys.stderr)
        sys.exit(2)
    subprocess.run([sys.executable, str(lut_py), str(inp)], check=True)


def prepare_encrypt(keygen_out: Path, encrypt_dir: Path, seed_d: int) -> Path:
    """准备 Encrypt input/；返回 m_ref 路径（input/m.bin）。"""
    ek_src = keygen_out / "ek_pke.bin"
    if not ek_src.is_file():
        print(f"[roundtrip_prepare] missing device ek: {ek_src}", file=sys.stderr)
        sys.exit(2)

    inp = encrypt_dir / "input"
    out = encrypt_dir / "output"
    inp.mkdir(parents=True, exist_ok=True)
    out.mkdir(parents=True, exist_ok=True)

    shutil.copy2(ek_src, inp / "ek_pke.bin")
    _write_m_coins(inp, seed_d)
    meta = struct.pack("<IIII", seed_d, EK_BYTES, MSG_BYTES, CT_BYTES)
    (inp / "meta.bin").write_bytes(meta)
    _run_ntt_lut_bins(encrypt_dir, inp)

    m_ref = inp / "m.bin"
    print(f"[roundtrip_prepare] encrypt input OK ek={EK_BYTES}B m+coins SEED_D={seed_d}")
    return m_ref


def prepare_decrypt(keygen_out: Path, decrypt_dir: Path, c_src: Path, seed_d: int) -> None:
    """准备 Decrypt input/：device dk + device c（来自 Encrypt output）。"""
    dk_src = keygen_out / "dk_pke.bin"
    if not dk_src.is_file():
        print(f"[roundtrip_prepare] missing device dk: {dk_src}", file=sys.stderr)
        sys.exit(2)
    if not c_src.is_file():
        print(f"[roundtrip_prepare] missing device c: {c_src}", file=sys.stderr)
        sys.exit(2)

    inp = decrypt_dir / "input"
    out = decrypt_dir / "output"
    inp.mkdir(parents=True, exist_ok=True)
    out.mkdir(parents=True, exist_ok=True)

    shutil.copy2(dk_src, inp / "dk_pke.bin")
    shutil.copy2(c_src, inp / "c.bin")
    meta = struct.pack("<IIII", seed_d, DK_BYTES, CT_BYTES, MSG_BYTES)
    (inp / "meta.bin").write_bytes(meta)
    _run_ntt_lut_bins(decrypt_dir, inp)
    print(f"[roundtrip_prepare] decrypt input OK dk={DK_BYTES}B c={CT_BYTES}B")


def main() -> None:
    ap = argparse.ArgumentParser(description="PKE Encrypt↔Decrypt round-trip input 准备")
    ap.add_argument("--mode", choices=("encrypt", "decrypt"), required=True)
    ap.add_argument("--keygen-out", type=Path, required=True, help="KeyGen output/（含 ek_pke.bin dk_pke.bin；默认 stable）")
    ap.add_argument("--encrypt-dir", type=Path, help="Encrypt 探针根目录")
    ap.add_argument("--decrypt-dir", type=Path, help="Decrypt 探针根目录")
    ap.add_argument("--c-src", type=Path, help="decrypt 模式：device Encrypt 产出的 c.bin")
    ap.add_argument("--seed-d", type=int, default=int(os.environ.get("SEED_D", str(SEED_D_DEFAULT))))
    args = ap.parse_args()

    keygen_out = args.keygen_out.resolve()
    if args.mode == "encrypt":
        if args.encrypt_dir is None:
            ap.error("--encrypt-dir required for encrypt mode")
        prepare_encrypt(keygen_out, args.encrypt_dir.resolve(), args.seed_d)
    else:
        if args.decrypt_dir is None or args.c_src is None:
            ap.error("--decrypt-dir and --c-src required for decrypt mode")
        prepare_decrypt(keygen_out, args.decrypt_dir.resolve(), args.c_src.resolve(), args.seed_d)


if __name__ == "__main__":
    main()
