#!/usr/bin/env python3
# @exp exp-mlkem-f203-pke-keygen-k4
# coding=utf-8
"""Host golden：仅写 output/golden_ek_pke.bin 与 golden_dk_pke.bin（KEYGEN_VERIFY 用）。"""
from __future__ import annotations

import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

from keygen_golden import build_full_keygen, write_golden_only  # noqa: E402

SEED_D_DEFAULT = 20260619


def main() -> None:
    seed_d = int(os.environ.get("SEED_D", str(SEED_D_DEFAULT)))
    mix_pass = int(os.environ.get("NTTS2S1E_MIX_PASS", os.environ.get("TAG5T_MIX_PASS", "0")))
    kg = build_full_keygen(seed_d, mix_pass=mix_pass)
    write_golden_only(ROOT, kg)
    print(
        f"[gen_data] SEED_D={seed_d} mixPass={mix_pass} "
        f"golden ek_pke={len(kg['ek_pke'])}B dk_pke={len(kg['dk_pke'])}B"
    )


if __name__ == "__main__":
    main()
