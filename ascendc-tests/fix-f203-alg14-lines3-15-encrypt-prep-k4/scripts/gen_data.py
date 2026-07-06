#!/usr/bin/env python3
# coding=utf-8
"""Golden 入口：读本探针 fixtures/input，写 output/golden_*.bin。

自包含：仅 import 本目录 scripts/golden_encrypt_prep.py 与 scripts/prep/alg7_geom.py。
"""
from __future__ import annotations

import hashlib
import os
import shutil
from pathlib import Path

from golden_encrypt_prep import XOF_BYTES, build_a_hat_from_rho, build_re_from_coins

ROOT = Path(__file__).resolve().parent.parent
FIXTURES_EK = ROOT / "fixtures" / "ek_pke.bin"
INPUT_EK = ROOT / "input" / "ek_pke.bin"

COINS_SEED_DEFAULT = 20260706
EK_PKE_BYTES = 1568
RHO_OFFSET = 1536
RHO_BYTES = 32


def coins_from_seed(coins_seed: int) -> bytes:
    msg = f"fix-f203-alg14-encrypt-prep:COINS_SEED={coins_seed}".encode()
    return hashlib.sha3_256(msg).digest()


def install_ek_pke(input_dir: Path) -> bytes:
    """将 fixtures/ek_pke.bin 安装到 input/；缺失则失败（不引用 stable 路径）。"""
    if not FIXTURES_EK.is_file() or FIXTURES_EK.stat().st_size != EK_PKE_BYTES:
        raise SystemExit(
            f"missing fixtures/ek_pke.bin ({EK_PKE_BYTES}B); "
            f"一次性从 stable KeyGen output 复制到 {FIXTURES_EK}"
        )
    input_dir.mkdir(parents=True, exist_ok=True)
    dst = input_dir / "ek_pke.bin"
    if not dst.is_file() or dst.stat().st_size != EK_PKE_BYTES:
        shutil.copy2(FIXTURES_EK, dst)
    ek = dst.read_bytes()
    if len(ek) != EK_PKE_BYTES:
        raise SystemExit(f"ek_pke size {len(ek)}")
    return ek


def main() -> None:
    coins_seed = int(os.environ.get("COINS_SEED", str(COINS_SEED_DEFAULT)))
    coins = coins_from_seed(coins_seed)

    input_dir = ROOT / "input"
    output_dir = ROOT / "output"
    output_dir.mkdir(exist_ok=True)

    ek = install_ek_pke(input_dir)
    rho = ek[RHO_OFFSET : RHO_OFFSET + RHO_BYTES]

    a_hat = build_a_hat_from_rho(rho)
    re = build_re_from_coins(coins)

    (input_dir / "coins.bin").write_bytes(coins)
    a_hat.tofile(output_dir / "golden_a_hat.bin")
    re.tofile(output_dir / "golden_re.bin")
    (output_dir / "golden_rho.bin").write_bytes(rho)

    print(
        f"[gen_data] ek_pke from fixtures→input ({EK_PKE_BYTES}B) "
        f"rho@{RHO_OFFSET} COINS_SEED={coins_seed} XOF_BYTES={XOF_BYTES} "
        f"a_hat={a_hat.shape} re={re.shape}"
    )


if __name__ == "__main__":
    main()
