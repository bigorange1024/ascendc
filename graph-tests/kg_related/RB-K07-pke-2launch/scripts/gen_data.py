#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RB-K04 gen_data：SEED_D → Host 预喂 seed/ρ/ζ/γ/mat；权威 golden = liboqs PKE KeyGen。

契约：
  - SEED_D=20260619 → Derand → d[32] → liboqs_pke_ref keygen → golden ek/dk
  - ρ 由同 Derand+G 派生，供 L2b Host 装填（与设备 prep 内 G 一致）
  - ζ/γ：只读 ntt_onnx 表；mat：非零极轻 Cube 输入
  - **缺 liboqs → SystemExit BLOCKED**（禁 python 冒充权威关闭门禁）
  - 禁预喂最终 ek/dk 到设备 input
"""
from __future__ import annotations

import os
import struct
import subprocess
import sys
from pathlib import Path

import numpy as np

_SCRIPT = Path(__file__).resolve().parent
_CASE = _SCRIPT.parent
_REPO = _CASE.parents[2]

SE_SHARED = _REPO / "library" / "shared" / "fips203_se_sample"
TOPO = _REPO / "graph-tests" / "enc_related" / "RB-T10-uv-device-mix" / "scripts"
sys.path.insert(0, str(SE_SHARED))
sys.path.insert(0, str(TOPO))

from golden_se_sampling import derand_bytes_from_seed  # noqa: E402
from topology_math import load_zetas_gammas  # noqa: E402

K = 4
SEED_D_DEFAULT = 20260619
EK_LEN = 1568
DK_LEN = 1536

REF_BIN = _REPO / "scripts" / "liboqs_pke_ref"
BUILD_REF = _REPO / "scripts" / "build_liboqs_pke_ref_mlkem1024.sh"


def _require_liboqs_ref() -> Path:
    """权威门禁：有则返回路径；可尝试 build；仍失败 → BLOCKED。"""
    if REF_BIN.is_file() and os.access(REF_BIN, os.X_OK):
        return REF_BIN
    if BUILD_REF.is_file():
        print(f"[gen_data] building {REF_BIN.name} via {BUILD_REF.name} …")
        try:
            subprocess.check_call(["bash", str(BUILD_REF)])
        except subprocess.CalledProcessError as e:
            raise SystemExit(
                f"[BLOCKED] build liboqs_pke_ref failed (rc={e.returncode}); "
                "install thirdparty/liboqs then retry"
            ) from e
        if REF_BIN.is_file() and os.access(REF_BIN, os.X_OK):
            return REF_BIN
    raise SystemExit(
        "[BLOCKED] liboqs_pke_ref missing; "
        "install thirdparty/liboqs then bash scripts/build_liboqs_pke_ref_mlkem1024.sh"
    )


def hash_g_rho(d: bytes) -> bytes:
    """G(d‖k) → ρ[32]（仅 Host 装填 L2b；非权威 oracle）。"""
    import hashlib

    buf = hashlib.sha3_512(d + bytes([K & 0xFF])).digest()
    return buf[:32]


def main() -> int:
    seed_d = int(os.environ.get("SEED_D", str(SEED_D_DEFAULT)))
    ref = _require_liboqs_ref()

    d = derand_bytes_from_seed(seed_d, kyber_k=K)
    rho = hash_g_rho(d)
    hex_d = d.hex()

    inp = _CASE / "input"
    out = _CASE / "output"
    inp.mkdir(parents=True, exist_ok=True)
    out.mkdir(parents=True, exist_ok=True)

    ek_path = out / "golden_ek_pke.bin"
    dk_path = out / "golden_dk_pke.bin"
    # 权威：liboqs PKE KeyGen(d)
    subprocess.check_call([str(ref), "keygen", str(ek_path), str(dk_path), hex_d])
    if ek_path.stat().st_size != EK_LEN or dk_path.stat().st_size != DK_LEN:
        raise SystemExit(
            f"[FAIL] liboqs keygen size ek={ek_path.stat().st_size} dk={dk_path.stat().st_size}"
        )

    zetas_list, gammas_list = load_zetas_gammas()
    zetas = np.asarray(zetas_list, dtype=np.int32)
    gammas = np.asarray(gammas_list, dtype=np.int32)
    rng = np.random.default_rng(20260909)
    mat_a = rng.integers(-8, 9, size=(16, 32), dtype=np.int8)
    mat_b = rng.integers(-8, 9, size=(32, 32), dtype=np.int8)

    (inp / "seed_d.bin").write_bytes(struct.pack("<I", seed_d))
    (inp / "rho.bin").write_bytes(rho)
    zetas.tofile(inp / "zetas.bin")
    gammas.tofile(inp / "gammas.bin")
    mat_a.tofile(inp / "mat_a.bin")
    mat_b.tofile(inp / "mat_b.bin")

    (out / "cross_backend.txt").write_text(
        f"liboqs_pke_ref:{ref}\nSEED_D={seed_d}\nhex_d={hex_d}\n", encoding="utf-8"
    )

    print(
        f"[gen_data] SEED_D={seed_d} oracle=liboqs_pke_ref "
        f"ek={EK_LEN} dk={DK_LEN} path={ref}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
