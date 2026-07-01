#!/usr/bin/env python3
"""
liboqs_pke_fixture.py — 生成 liboqs PKE 黑盒向量（ml_kem_1024 / k=4）。

输出目录（默认 output/liboqs_pke_fixture/<SEED_D>/）：
  seed_d.bin   — uint32 LE（与 KeyGen 探针 input 一致）
  d.bin        — derand 32B（FIPS coins / indcpa keygen 种子）
  m.bin        — 32B 明文（与 Encrypt gen_data 同规则：RNG seed_d+991）
  coins.bin    — 32B Encrypt 随机性
  ek_pke.bin   — liboqs 公钥 1568B
  dk_pke.bin   — liboqs 私钥 1536B
  c.bin        — liboqs 密文 1568B
  m_rec.bin    — liboqs 解密恢复 32B（须与 m.bin 一致）
"""
from __future__ import annotations

import argparse
import os
import struct
import subprocess
import sys
from pathlib import Path

import numpy as np

REPO_ROOT = Path(__file__).resolve().parent.parent
FIPS203_SE = REPO_ROOT / "library/shared/fips203_se_sample"
REF_BIN = REPO_ROOT / "scripts/liboqs_pke_ref"
BUILD_REF = REPO_ROOT / "scripts/build_liboqs_pke_ref.sh"

sys.path.insert(0, str(FIPS203_SE))
from golden_se_sampling import derand_bytes_from_seed  # noqa: E402

EK_BYTES = 1568
DK_BYTES = 1536
CT_BYTES = 1568
MSG_BYTES = 32
COINS_BYTES = 32
SEED_D_DEFAULT = 20260619


def _ensure_ref() -> Path:
    if REF_BIN.is_file():
        return REF_BIN
    subprocess.check_call(["bash", str(BUILD_REF)])
    return REF_BIN


def _write_m_coins(out: Path, seed_d: int) -> None:
    rng = np.random.default_rng(seed_d + 991)
    m = rng.integers(0, 256, size=MSG_BYTES, dtype=np.uint8)
    coins = rng.integers(0, 256, size=COINS_BYTES, dtype=np.uint8)
    m.tofile(out / "m.bin")
    coins.tofile(out / "coins.bin")


def _run_ref(args: list[str]) -> None:
    subprocess.check_call([str(_ensure_ref()), *args])


def generate_fixture(out_dir: Path, seed_d: int) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    d = derand_bytes_from_seed(seed_d)
    (out_dir / "seed_d.bin").write_bytes(struct.pack("<I", seed_d))
    (out_dir / "d.bin").write_bytes(d)
    _write_m_coins(out_dir, seed_d)

    ek = out_dir / "ek_pke.bin"
    dk = out_dir / "dk_pke.bin"
    c = out_dir / "c.bin"
    m_rec = out_dir / "m_rec.bin"

    _run_ref(["keygen", str(ek), str(dk), d.hex()])
    _run_ref(["encrypt", str(c), str(ek), str(out_dir / "m.bin"), str(out_dir / "coins.bin")])
    _run_ref(["decrypt", str(m_rec), str(dk), str(c)])

    for name, size in (
        ("ek_pke.bin", EK_BYTES),
        ("dk_pke.bin", DK_BYTES),
        ("c.bin", CT_BYTES),
        ("m.bin", MSG_BYTES),
        ("m_rec.bin", MSG_BYTES),
    ):
        p = out_dir / name
        if p.stat().st_size != size:
            raise SystemExit(f"[liboqs_fixture] bad size {name}={p.stat().st_size} want {size}")

    m = np.fromfile(out_dir / "m.bin", dtype=np.uint8)
    mr = np.fromfile(m_rec, dtype=np.uint8)
    if not np.array_equal(m, mr):
        idx = int(np.argmax(m != mr))
        raise SystemExit(f"[liboqs_fixture] liboqs round-trip m_rec != m @ {idx}")

    print(f"[liboqs_fixture] OK SEED_D={seed_d} -> {out_dir}")


def main() -> None:
    ap = argparse.ArgumentParser(description="liboqs PKE fixture 生成")
    ap.add_argument("--seed-d", type=int, default=int(os.environ.get("SEED_D", str(SEED_D_DEFAULT))))
    ap.add_argument(
        "--out-dir",
        type=Path,
        default=None,
        help="默认 output/liboqs_pke_fixture/<SEED_D>/",
    )
    args = ap.parse_args()
    out = args.out_dir or (REPO_ROOT / "output" / "liboqs_pke_fixture" / str(args.seed_d))
    generate_fixture(out.resolve(), args.seed_d)


if __name__ == "__main__":
    main()
