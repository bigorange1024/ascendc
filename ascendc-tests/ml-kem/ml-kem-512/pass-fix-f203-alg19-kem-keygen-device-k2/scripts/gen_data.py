#!/usr/bin/env python3
"""
gen_data.py — Alg.19 KEM KeyGen 的 ML-KEM-512 host golden 生成器。

用途（仅 KEM_KEYGEN_VERIFY=1 / 对拍路径）：
  复用活跃 D13 k2 PKE KeyGen oracle 生成 ek_pke/dk_pke，再按 FIPS 203
  KEM 展开布局拼接 dk_kem = dk_pke‖ek‖H(ek)‖z。

本脚本只提供 I/O oracle；禁止把 Python 计算过程当作 AscendC 实现规格。
"""
from __future__ import annotations

import hashlib
import importlib.util
import os
import struct
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parent.parent


def _ascendc_repo_root(start: Path) -> Path:
    """自 start 向上查找仓库根，兼容 ml-kem 参数组嵌套目录。"""
    p = start.resolve()
    for d in [p, *p.parents]:
        if (d / "AGENTS.md").is_file() and (d / "scripts").is_dir():
            return d
    raise RuntimeError(f"cannot locate ascendc repo root from {start}")


REPO = _ascendc_repo_root(ROOT)
D13_ROOT = REPO / "ascendc-tests/ml-kem/ml-kem-512/pass-fix-f203-alg13-device-keygen-k2"
_KEYGEN_GOLDEN = D13_ROOT / "scripts/keygen_golden.py"

_spec = importlib.util.spec_from_file_location("mlkem512_d13_keygen_golden", _KEYGEN_GOLDEN)
keygen_golden = importlib.util.module_from_spec(_spec)
assert _spec.loader is not None
_spec.loader.exec_module(keygen_golden)

EK_KEM_BYTES = 800
DK_PKE_BYTES = 768
DK_KEM_BYTES = 1632
SEED_D_DEFAULT = 20260619


def derand_z_from_seed(seed_d: int) -> bytes:
    """与设备 DerandZFromSeedD 同式：SHA3-256("exp-mlkem-f203-kem-k2:SEED_Z="‖十进制)。"""
    msg = f"exp-mlkem-f203-kem-k2:SEED_Z={seed_d}".encode()
    return hashlib.sha3_256(msg).digest()


def build_kem_keygen(seed_d: int) -> dict[str, Any]:
    """构造 Alg.19 输出：ek_kem=ek_pke，dk_kem=dk_pke‖ek‖H(ek)‖z。"""
    kg = keygen_golden.build_full_keygen(seed_d, mix_pass=0)
    ek = bytes(kg["ek_pke"])
    dk_pke = bytes(kg["dk_pke"])
    z = derand_z_from_seed(seed_d)
    h_ek = hashlib.sha3_256(ek).digest()
    dk_kem = dk_pke + ek + h_ek + z
    if len(ek) != EK_KEM_BYTES or len(dk_pke) != DK_PKE_BYTES or len(dk_kem) != DK_KEM_BYTES:
        raise RuntimeError(f"bad KEM sizes ek={len(ek)} dk_pke={len(dk_pke)} dk_kem={len(dk_kem)}")
    return {"seed_d": seed_d, "ek": ek, "dk_pke": dk_pke, "h_ek": h_ek, "z": z, "dk_kem": dk_kem}


def main() -> None:
    """写 seed_d.bin，并生成 golden ek_kem/dk_kem。"""
    seed_d = int(os.environ.get("SEED_D", str(SEED_D_DEFAULT)))
    kem = build_kem_keygen(seed_d)

    inp = ROOT / "input"
    inp.mkdir(exist_ok=True)
    (inp / "seed_d.bin").write_bytes(struct.pack("<I", seed_d))

    out_dir = ROOT / "output"
    out_dir.mkdir(exist_ok=True)
    (out_dir / "golden_ek_kem.bin").write_bytes(kem["ek"])
    (out_dir / "golden_dk_kem.bin").write_bytes(kem["dk_kem"])
    (out_dir / "golden_dk_pke.bin").write_bytes(kem["dk_pke"])
    (out_dir / "golden_h_ek.bin").write_bytes(kem["h_ek"])
    (out_dir / "golden_z.bin").write_bytes(kem["z"])

    print(f"[gen_data] SEED_D={seed_d} golden ek_kem={EK_KEM_BYTES}B dk_kem={DK_KEM_BYTES}B")


if __name__ == "__main__":
    main()
