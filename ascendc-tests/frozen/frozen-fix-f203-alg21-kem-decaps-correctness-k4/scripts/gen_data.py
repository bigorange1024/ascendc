#!/usr/bin/env python3
"""
gen_data.py — FIPS 203 Alg.21 Decaps 生产 input / 可选 golden。

生产（默认 run.sh）：
  - 从 Alg.19 复制 dk_kem.bin（DK_KEM_SRC）
  - 从 Alg.20 复制 c.bin（C_SRC）
  - 调 host_golden/ntt_lut_bins.py 写 LUT

VERIFY（KEM_DECAPS_VERIFY=1）：
  - golden_K ← Alg.20 Encaps 的 K.bin（合法路径）
  - golden_K_reject ← J(z‖c)=SHAKE256（拒绝路径对拍）
禁止 Host 跑 indcpa_dec/enc 冒充设备 FO。
"""
from __future__ import annotations

import hashlib
import os
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DK_BYTES = 3168
CT_BYTES = 1568
K_BYTES = 32
DK_KEM_DEFAULT = ROOT.parent / "frozen-fix-f203-alg19-kem-keygen-correctness-k4" / "output" / "dk_kem.bin"
C_DEFAULT = ROOT.parent / "frozen-fix-f203-alg20-kem-encaps-correctness-k4" / "output" / "c.bin"
K_ENC_DEFAULT = ROOT.parent / "frozen-fix-f203-alg20-kem-encaps-correctness-k4" / "output" / "K.bin"
HOST_GOLDEN = ROOT / "scripts" / "host_golden"


def main() -> None:
    """复制 dk/c、写 LUT；可选写 golden_K / golden_K_reject。"""
    dk_src = Path(os.environ.get("DK_KEM_SRC", str(DK_KEM_DEFAULT)))
    c_src = Path(os.environ.get("C_SRC", str(C_DEFAULT)))
    inp = ROOT / "input"
    out = ROOT / "output"
    inp.mkdir(parents=True, exist_ok=True)
    out.mkdir(parents=True, exist_ok=True)

    if not dk_src.is_file():
        print(f"[gen_data] missing dk_kem source: {dk_src} (run alg19 KeyGen first)", file=sys.stderr)
        sys.exit(2)
    if not c_src.is_file():
        print(f"[gen_data] missing c source: {c_src} (run alg20 Encaps first)", file=sys.stderr)
        sys.exit(3)

    dk_dst = inp / "dk_kem.bin"
    c_dst = inp / "c.bin"
    if dk_src.resolve() != dk_dst.resolve():
        shutil.copy2(dk_src, dk_dst)
    if c_src.resolve() != c_dst.resolve():
        shutil.copy2(c_src, c_dst)

    import subprocess

    subprocess.run([sys.executable, str(HOST_GOLDEN / "ntt_lut_bins.py"), str(inp)], check=True)

    if os.environ.get("KEM_DECAPS_VERIFY", "0") == "1":
        k_src = Path(os.environ.get("K_ENC_SRC", str(K_ENC_DEFAULT)))
        if not k_src.is_file():
            print(f"[gen_data] missing K golden source: {k_src}", file=sys.stderr)
            sys.exit(4)
        shutil.copy2(k_src, out / "golden_K.bin")
        dk = (inp / "dk_kem.bin").read_bytes()
        c = (inp / "c.bin").read_bytes()
        z = dk[3136:3168]
        (out / "golden_K_reject.bin").write_bytes(hashlib.shake_256(z + c).digest(32))
        print(f"[gen_data] golden_K from {k_src}")
        print("[gen_data] golden_K_reject = J(z||c)")
    else:
        print(f"[gen_data] dk_kem from {dk_src}, c from {c_src}")


if __name__ == "__main__":
    main()
