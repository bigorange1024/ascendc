#!/usr/bin/env python3
"""gen_data — 生产 input only（Alg.15 Decrypt）。"""
from __future__ import annotations

import os
import struct
import subprocess
import sys
from pathlib import Path

import numpy as np

SCRIPT_DIR = Path(__file__).resolve().parent
CASE_DIR = SCRIPT_DIR.parent
HOST_GOLDEN = SCRIPT_DIR / "host_golden"

DK_BYTES = 1536
CT_BYTES = 1568
MSG_BYTES = 32
SEED_D_DEFAULT = 20260619


def main() -> None:
    seed_d = int(os.environ.get("SEED_D", str(SEED_D_DEFAULT)))
    inp = CASE_DIR / "input"
    out = CASE_DIR / "output"
    inp.mkdir(parents=True, exist_ok=True)
    out.mkdir(parents=True, exist_ok=True)

    subprocess.run(
        [sys.executable, str(HOST_GOLDEN / "gen_dk_pke.py"), str(seed_d), str(inp / "dk_pke.bin")],
        check=True,
    )

    ek_path = inp / "ek_pke.bin"
    subprocess.run(
        [sys.executable, str(HOST_GOLDEN / "gen_ek_pke.py"), str(seed_d), str(ek_path)],
        check=True,
    )

    rng = np.random.default_rng(seed_d + 991)
    m = rng.integers(0, 256, size=MSG_BYTES, dtype=np.uint8)
    coins = rng.integers(0, 256, size=32, dtype=np.uint8)
    m.tofile(inp / "m.bin")
    coins.tofile(inp / "coins.bin")

    subprocess.run(
        [
            sys.executable,
            str(HOST_GOLDEN / "golden_c.py"),
            str(ek_path),
            str(inp / "m.bin"),
            str(inp / "coins.bin"),
            str(inp / "c.bin"),
        ],
        check=True,
    )

    meta = struct.pack("<IIII", seed_d, DK_BYTES, CT_BYTES, MSG_BYTES)
    (inp / "meta.bin").write_bytes(meta)

    subprocess.run([sys.executable, str(HOST_GOLDEN / "ntt_lut_bins.py"), str(inp)], check=True)

    if os.environ.get("DECRYPT_VERIFY", "1") == "1":
        subprocess.run(
            [
                sys.executable,
                str(HOST_GOLDEN / "golden_m.py"),
                str(inp / "dk_pke.bin"),
                str(inp / "c.bin"),
                str(out / "golden_m.bin"),
            ],
            check=True,
        )
        print(f"[gen_data] SEED_D={seed_d} golden_m OK")


if __name__ == "__main__":
    main()
