#!/usr/bin/env python3
"""
gen_data.py — Alg.20 Encaps 生产 input。

- ek_kem.bin：从 alg19 KeyGen output 复制（EK_KEM_SRC）
- seed_d.bin：SEED_D
- LUT：host_golden/ntt_lut_bins.py
- golden c/K：仅 KEM_ENCAPS_VERIFY=1（liboqs encaps_derand）
"""
from __future__ import annotations

import hashlib
import os
import shutil
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
REPO = ROOT.parent.parent
HOST_GOLDEN = ROOT / "scripts" / "host_golden"
EK_BYTES = 1568
CT_BYTES = 1568
K_BYTES = 32
SEED_D_DEFAULT = 20260619
EK_KEM_DEFAULT = ROOT.parent / "fix-f203-alg19-kem-keygen-k4" / "output" / "ek_kem.bin"


def derand_m_from_seed(seed_d: int) -> bytes:
    msg = f"exp-mlkem-f203-kem-encaps-k4:SEED_M={seed_d}".encode()
    return hashlib.sha3_256(msg).digest()


def main() -> None:
    seed_d = int(os.environ.get("SEED_D", str(SEED_D_DEFAULT)))
    ek_src = Path(os.environ.get("EK_KEM_SRC", str(EK_KEM_DEFAULT)))
    inp = ROOT / "input"
    out = ROOT / "output"
    inp.mkdir(parents=True, exist_ok=True)
    out.mkdir(parents=True, exist_ok=True)

    if not ek_src.is_file():
        print(f"[gen_data] missing ek_kem source: {ek_src} (run alg19 KeyGen first)", file=sys.stderr)
        sys.exit(2)
    ek_dst = inp / "ek_kem.bin"
    if ek_src.resolve() != ek_dst.resolve():
        shutil.copy2(ek_src, ek_dst)
    if os.environ.get("KEM_ENC_EXT_SEED", "0") == "1":
        if not (inp / "encaps_seed.bin").is_file():
            print("[gen_data] KEM_ENC_EXT_SEED=1 requires input/encaps_seed.bin (32B)", file=sys.stderr)
            sys.exit(2)
    else:
        (inp / "seed_d.bin").write_bytes(struct.pack("<I", seed_d))

    subprocess.run([sys.executable, str(HOST_GOLDEN / "ntt_lut_bins.py"), str(inp)], check=True)

    if os.environ.get("KEM_ENCAPS_VERIFY", "0") == "1":
        m = derand_m_from_seed(seed_d)
        ek = (inp / "ek_kem.bin").read_bytes()
        ref = REPO / "scripts" / "liboqs_kem_ref"
        if not ref.is_file():
            subprocess.check_call(["bash", str(REPO / "scripts" / "build_liboqs_kem_ref.sh")])
        # 扩展 encaps 子命令（若未编译则下面会失败）
        ct_path = out / "golden_c.bin"
        k_path = out / "golden_K.bin"
        subprocess.check_call(
            [str(ref), "encaps", str(inp / "ek_kem.bin"), str(ct_path), str(k_path), m.hex()]
        )
        print(f"[gen_data] SEED_D={seed_d} golden c/K via liboqs encaps_derand")
    else:
        print(f"[gen_data] SEED_D={seed_d} ek_kem from {ek_src}")


if __name__ == "__main__":
    main()
