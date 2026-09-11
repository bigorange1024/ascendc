#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RB-T25 gen_data：liboqs PKE KeyGen+Encrypt → dk_pke/c；golden_m ← liboqs Decrypt。

契约：
  - 权威 golden：scripts/liboqs_pke_ref decrypt（缺库 → SystemExit BLOCKED）
  - Host 仅落盘 dk_pke + c + ζ/γ/mat；**不**预喂最终 m 给设备 input
  - SEED 可复现；禁抄 decrypt/encrypt 设备树造数逻辑
"""
from __future__ import annotations

import os
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np

_SCRIPT = Path(__file__).resolve().parent
_CASE = _SCRIPT.parent
_REPO = _CASE.parents[2]

sys.path.insert(0, str(_REPO / "graph-tests" / "enc_related" / "RB-T10-uv-device-mix" / "scripts"))
from topology_math import load_zetas_gammas  # noqa: E402

SEED_D = int(os.environ.get("SEED_D", "20260908"))
DK_BYTES = 1536
C_BYTES = 1568
M_BYTES = 32
EK_BYTES = 1568
COINS_BYTES = 32

REF_BIN = _REPO / "scripts" / "liboqs_pke_ref"
BUILD_REF = _REPO / "scripts" / "build_liboqs_pke_ref_mlkem1024.sh"


def _ensure_ref() -> Path:
    """确保 liboqs PKE ref 可执行；缺库则 BLOCKED。"""
    if REF_BIN.is_file() and os.access(REF_BIN, os.X_OK):
        return REF_BIN
    if not BUILD_REF.is_file():
        raise SystemExit(
            "[BLOCKED] liboqs_pke_ref missing and no build script; "
            "install thirdparty/liboqs then bash scripts/build_liboqs_pke_ref_mlkem1024.sh"
        )
    print(f"[gen_data] building {REF_BIN} via {BUILD_REF.name} …")
    try:
        subprocess.check_call(["bash", str(BUILD_REF)])
    except subprocess.CalledProcessError as e:
        raise SystemExit(f"[BLOCKED] build liboqs_pke_ref failed: {e}") from e
    if not REF_BIN.is_file():
        raise SystemExit("[BLOCKED] liboqs_pke_ref still missing after build")
    return REF_BIN


def _derand_d(seed: int) -> bytes:
    """与 fips203_se_sample derand 同构：SHAKE 式可复现 32B（简化：固定 RNG）。"""
    rng = np.random.default_rng(seed)
    return bytes(rng.integers(0, 256, size=32, dtype=np.uint8).tolist())


def main() -> int:
    inp = _CASE / "input"
    out = _CASE / "output"
    inp.mkdir(parents=True, exist_ok=True)
    out.mkdir(parents=True, exist_ok=True)

    ref = _ensure_ref()
    d = _derand_d(SEED_D)
    d_hex = d.hex()

    rng = np.random.default_rng(SEED_D + 991)
    m = bytes(rng.integers(0, 256, size=M_BYTES, dtype=np.uint8).tolist())
    coins = bytes(rng.integers(0, 256, size=COINS_BYTES, dtype=np.uint8).tolist())

    with tempfile.TemporaryDirectory(prefix="rb_t25_") as td:
        tdp = Path(td)
        ek = tdp / "ek.bin"
        dk = tdp / "dk.bin"
        c = tdp / "c.bin"
        m_path = tdp / "m.bin"
        coins_path = tdp / "coins.bin"
        m_dec = tdp / "m_dec.bin"
        m_path.write_bytes(m)
        coins_path.write_bytes(coins)

        subprocess.check_call([str(ref), "keygen", str(ek), str(dk), d_hex])
        if ek.stat().st_size != EK_BYTES or dk.stat().st_size != DK_BYTES:
            raise SystemExit(
                f"[BLOCKED] keygen size ek={ek.stat().st_size} dk={dk.stat().st_size}"
            )
        subprocess.check_call(
            [str(ref), "encrypt", str(c), str(ek), str(m_path), str(coins_path)]
        )
        if c.stat().st_size != C_BYTES:
            raise SystemExit(f"[BLOCKED] encrypt c size={c.stat().st_size}")
        subprocess.check_call([str(ref), "decrypt", str(m_dec), str(dk), str(c)])
        golden_m = m_dec.read_bytes()
        if len(golden_m) != M_BYTES:
            raise SystemExit(f"[BLOCKED] decrypt m size={len(golden_m)}")
        if golden_m != m:
            raise SystemExit("[BLOCKED] liboqs decrypt(m) != plaintext (fixture broken)")

        (inp / "dk_pke.bin").write_bytes(dk.read_bytes())
        (inp / "c.bin").write_bytes(c.read_bytes())
        (out / "golden_m.bin").write_bytes(golden_m)
        # 诊断：明文副本（非设备 input）
        (out / "plain_m.bin").write_bytes(m)

    zetas_list, gammas_list = load_zetas_gammas()
    np.asarray(zetas_list, dtype=np.int32).tofile(inp / "zetas.bin")
    np.asarray(gammas_list, dtype=np.int32).tofile(inp / "gammas.bin")

    cube_rng = np.random.default_rng(20260908)
    cube_rng.integers(-8, 9, size=(16, 32), dtype=np.int8).tofile(inp / "mat_a.bin")
    cube_rng.integers(-8, 9, size=(32, 32), dtype=np.int8).tofile(inp / "mat_b.bin")

    print(
        f"[gen_data] SEED_D={SEED_D} dk={DK_BYTES} c={C_BYTES} "
        f"golden_m via liboqs_pke_ref decrypt OK"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
