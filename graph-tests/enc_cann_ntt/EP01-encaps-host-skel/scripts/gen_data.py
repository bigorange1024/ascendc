#!/usr/bin/python3
# coding=utf-8
"""
EP01-encaps-host-skel · Encaps Host 壳造数

产出：
  input/ek_kem.bin（1568）— liboqs KeyGen
  input/m.bin（32）— 定点 derand
  output/golden_K.bin / golden_r.bin / golden_h.bin — Host SHA3 期望（verify soft）
"""

from __future__ import annotations

import hashlib
import os
import sys
from pathlib import Path

import numpy as np

_REPO = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(_REPO / "scripts"))
sys.path.insert(0, str(_REPO / "library" / "shared" / "f203_kem_ref"))

from kem_ref import derand_d, derand_z, kem_keygen  # noqa: E402
from liboqs_kem_fixture import derand_m_from_seed  # noqa: E402

SEED_D = int(os.environ.get("SEED_D", "20260619"))
EK_BYTES = 1568
M_BYTES = 32


def main() -> None:
    Path("input").mkdir(exist_ok=True)
    Path("output").mkdir(exist_ok=True)

    ek_path = Path("./input/ek_kem.bin")
    dk_path = Path("./input/dk_kem.bin")
    kem_seed = derand_d(SEED_D) + derand_z(SEED_D)
    src = kem_keygen(kem_seed, ek_path, dk_path)
    ek = ek_path.read_bytes()
    if len(ek) != EK_BYTES:
        raise SystemExit(f"ek size {len(ek)} != {EK_BYTES}")

    m = derand_m_from_seed(SEED_D, 4)
    Path("./input/m.bin").write_bytes(m)
    print(f"[INFO] ek via {src}; m derand SEED_D={SEED_D}")

    h = hashlib.sha3_256(ek).digest()
    g = hashlib.sha3_512(m + h).digest()
    k_bar, r = g[:32], g[32:]
    Path("./output/golden_h.bin").write_bytes(h)
    Path("./output/golden_K.bin").write_bytes(k_bar)
    Path("./output/golden_r.bin").write_bytes(r)
    # 桩 c 期望：与 main 同式 c[i]=r[i%32]^(i&0xff)
    c_stub = bytes((r[i % 32] ^ (i & 0xFF)) for i in range(1568))
    Path("./output/golden_c_stub.bin").write_bytes(c_stub)
    print("[OK] EP01 encaps-host-skel inputs written")


if __name__ == "__main__":
    main()
