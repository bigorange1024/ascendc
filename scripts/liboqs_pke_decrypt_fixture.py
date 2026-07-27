#!/usr/bin/env python3
"""
liboqs_pke_decrypt_fixture.py — Decrypt KAT 夹具（规避 liboqs_pke_ref_mlkem1024 encrypt/decrypt 链接问题）。

流程：
  1. liboqs keygen（OQS_KEM_ml_kem_1024_keypair_derand）→ ek/dk
  2. RNG(seed_d+991) → m/coins（与 Encrypt gen_data / liboqs_pke_fixture 同规则）
  3. host golden_c.py(ek,m,coins) → c
  4. 可选：host golden_m(dk,c) 自检 m

输出目录（默认 output/liboqs_decrypt_fixture/<SEED_D>/）：
  seed_d.bin, d.bin, m.bin, coins.bin, ek_pke.bin, dk_pke.bin, c.bin
"""
from __future__ import annotations

import argparse
import os
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np

REPO_ROOT = Path(__file__).resolve().parent.parent
FIPS203_SE = REPO_ROOT / "library/shared/fips203_se_sample"
KEYGEN_SCRIPTS = REPO_ROOT / "examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-keygen-k4/scripts"
DECRYPT_HOST_GOLDEN = (
    REPO_ROOT / "examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-decrypt-k4/scripts/host_golden"
)
REF_BIN = KEYGEN_SCRIPTS / "liboqs_pke_keygen_ref"
BUILD_REF = KEYGEN_SCRIPTS / "build_liboqs_pke_keygen_ref.sh"

sys.path.insert(0, str(FIPS203_SE))
from golden_se_sampling import derand_bytes_from_seed  # noqa: E402

EK_BYTES = 1568
DK_BYTES = 1536
CT_BYTES = 1568
MSG_BYTES = 32
COINS_BYTES = 32
SEED_D_DEFAULT = 20260619


def _ensure_keygen_ref() -> Path:
    if REF_BIN.is_file():
        return REF_BIN
    subprocess.check_call(["bash", str(BUILD_REF)])
    return REF_BIN


def _run_keygen(d: bytes, ek: Path, dk: Path) -> None:
    subprocess.check_call([str(_ensure_keygen_ref()), str(ek), str(dk), d.hex()])


def _write_m_coins(out: Path, seed_d: int) -> None:
    rng = np.random.default_rng(seed_d + 991)
    m = rng.integers(0, 256, size=MSG_BYTES, dtype=np.uint8)
    coins = rng.integers(0, 256, size=COINS_BYTES, dtype=np.uint8)
    m.tofile(out / "m.bin")
    coins.tofile(out / "coins.bin")


def _host_golden_c(ek: Path, m: Path, coins: Path, c_out: Path) -> None:
    subprocess.check_call(
        [
            sys.executable,
            str(DECRYPT_HOST_GOLDEN / "golden_c.py"),
            str(ek),
            str(m),
            str(coins),
            str(c_out),
        ]
    )


def _host_golden_m(dk: bytes, c: bytes) -> bytes:
    with tempfile.TemporaryDirectory() as td:
        tdp = Path(td)
        (tdp / "dk.bin").write_bytes(dk)
        (tdp / "c.bin").write_bytes(c)
        out_m = tdp / "m.bin"
        subprocess.check_call(
            [
                sys.executable,
                str(DECRYPT_HOST_GOLDEN / "golden_m.py"),
                str(tdp / "dk.bin"),
                str(tdp / "c.bin"),
                str(out_m),
            ]
        )
        return out_m.read_bytes()


def generate_fixture(out_dir: Path, seed_d: int) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    d = derand_bytes_from_seed(seed_d)
    (out_dir / "seed_d.bin").write_bytes(struct.pack("<I", seed_d))
    (out_dir / "d.bin").write_bytes(d)
    _write_m_coins(out_dir, seed_d)

    ek = out_dir / "ek_pke.bin"
    dk = out_dir / "dk_pke.bin"
    c = out_dir / "c.bin"
    _run_keygen(d, ek, dk)
    _host_golden_c(ek, out_dir / "m.bin", out_dir / "coins.bin", c)

    for name, size in (
        ("ek_pke.bin", EK_BYTES),
        ("dk_pke.bin", DK_BYTES),
        ("c.bin", CT_BYTES),
        ("m.bin", MSG_BYTES),
    ):
        p = out_dir / name
        if p.stat().st_size != size:
            raise SystemExit(f"[decrypt_fixture] bad size {name}={p.stat().st_size} want {size}")

    # host golden_m 须与 RNG m 一致（闭环自检）
    m = (out_dir / "m.bin").read_bytes()
    m_host = _host_golden_m(dk.read_bytes(), c.read_bytes())
    if m != m_host:
        idx = next(i for i, (a, b) in enumerate(zip(m, m_host)) if a != b)
        raise SystemExit(f"[decrypt_fixture] host golden_m != RNG m @ {idx}")

    print(f"[decrypt_fixture] OK SEED_D={seed_d} -> {out_dir}")


def main() -> None:
    ap = argparse.ArgumentParser(description="Decrypt KAT fixture（liboqs keygen + host golden_c）")
    ap.add_argument("--seed-d", type=int, default=int(os.environ.get("SEED_D", str(SEED_D_DEFAULT))))
    ap.add_argument(
        "--out-dir",
        type=Path,
        default=None,
        help="默认 output/liboqs_decrypt_fixture/<SEED_D>/",
    )
    args = ap.parse_args()
    out = args.out_dir or (REPO_ROOT / "output" / "liboqs_decrypt_fixture" / str(args.seed_d))
    generate_fixture(out.resolve(), args.seed_d)


if __name__ == "__main__":
    main()
