#!/usr/bin/env python3
"""gen_data — RB-K05-kem-tail：ek/dk_pke + SEED_D → golden H(ek)/z/dk_kem。

本脚本是黑盒 host oracle，不是 AscendC 实现规格。

布局（liboqs / Alg.19 note）：
  dk_kem(3168) = dk_pke(1536) ‖ ek(1568) ‖ H(ek)(32) ‖ z(32)
  H(ek) = SHA3-256(ek)
  z     = SHA3-256(b"exp-mlkem-f203-kem-k4:SEED_Z={SEED_D}")
          （scripts/liboqs_kem_fixture.py::derand_z_from_seed，k=4）

上游：优先拷贝 RB-K04-pke-full/output/{ek_pke,dk_pke}.bin（同 SEED_D=20260619）；
若缺失则同 SEED 自洽生成确定性假 ek/dk_pke（仅验尾段拼接，不冒充 PKE 权威）。
Host **不**把 golden 喂给设备——仅写 output/golden_*.bin 供 verify。
"""
from __future__ import annotations

import hashlib
import os
import struct
import sys

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_CASE_DIR = os.path.normpath(os.path.join(_SCRIPT_DIR, ".."))
_REPO = os.path.normpath(os.path.join(_CASE_DIR, "../../.."))
_K04_OUT = os.path.normpath(os.path.join(_CASE_DIR, "../RB-K04-pke-full/output"))

SEED_D = 20260619
EK_LEN = 1568
DK_PKE_LEN = 1536
HASH_LEN = 32
DK_KEM_LEN = 3168
SEED_PAD = 32


def derand_z_from_seed(seed_d: int) -> bytes:
    """对齐 liboqs_kem_fixture.derand_z_from_seed(k=4)。"""
    msg = f"exp-mlkem-f203-kem-k4:SEED_Z={seed_d}".encode()
    return hashlib.sha3_256(msg).digest()


def host_oracle(ek: bytes, dk_pke: bytes, seed_d: int) -> tuple[bytes, bytes, bytes]:
    """H(ek) + z + 拼接 dk_kem。"""
    assert len(ek) == EK_LEN and len(dk_pke) == DK_PKE_LEN
    h = hashlib.sha3_256(ek).digest()
    z = derand_z_from_seed(seed_d)
    dk_kem = dk_pke + ek + h + z
    assert len(dk_kem) == DK_KEM_LEN
    return h, z, dk_kem


def load_or_synth_pke() -> tuple[bytes, bytes, str]:
    """优先 RB-K04 产物；否则确定性自洽合成。"""
    ek_path = os.path.join(_K04_OUT, "ek_pke.bin")
    dk_path = os.path.join(_K04_OUT, "dk_pke.bin")
    if os.path.isfile(ek_path) and os.path.isfile(dk_path):
        ek = open(ek_path, "rb").read()
        dk = open(dk_path, "rb").read()
        if len(ek) == EK_LEN and len(dk) == DK_PKE_LEN:
            return ek, dk, f"upstream=RB-K04:{_K04_OUT}"
    # 自洽假钥：按 SEED_D 域分离填充，仅供尾段对拍
    ek = hashlib.shake_256(f"RB-K05-synth-ek:SEED_D={SEED_D}".encode()).digest(EK_LEN)
    dk = hashlib.shake_256(f"RB-K05-synth-dk:SEED_D={SEED_D}".encode()).digest(DK_PKE_LEN)
    return ek, dk, "upstream=synth_same_SEED_D"


def try_liboqs_note() -> str:
    """可选加分：探测 liboqs；本刀关闸仍是 host oracle。"""
    candidates = [
        os.path.join(_REPO, "thirdparty/liboqs/build/lib/liboqs.so"),
        os.path.join(_REPO, "thirdparty/liboqs/build/lib/liboqs.so.0"),
    ]
    for p in candidates:
        if os.path.isfile(p):
            return f"liboqs_so_present:{p};gate=host_oracle"
    return "gate=host_oracle (liboqs optional bonus for KGR-K02)"


def main() -> None:
    inp = os.path.join(_CASE_DIR, "input")
    out = os.path.join(_CASE_DIR, "output")
    os.makedirs(inp, exist_ok=True)
    os.makedirs(out, exist_ok=True)

    ek, dk_pke, src = load_or_synth_pke()
    h, z, dk_kem = host_oracle(ek, dk_pke, SEED_D)
    seed_pad = bytearray(SEED_PAD)
    seed_pad[0:4] = struct.pack("<I", SEED_D)

    open(os.path.join(inp, "ek_pke.bin"), "wb").write(ek)
    open(os.path.join(inp, "dk_pke.bin"), "wb").write(dk_pke)
    open(os.path.join(inp, "seed_d.bin"), "wb").write(bytes(seed_pad))

    open(os.path.join(out, "golden_h.bin"), "wb").write(h)
    open(os.path.join(out, "golden_z.bin"), "wb").write(z)
    open(os.path.join(out, "golden_dk_kem.bin"), "wb").write(dk_kem)

    note = try_liboqs_note()
    with open(os.path.join(out, "oracle_note.txt"), "w", encoding="utf-8") as f:
        f.write(note + "\n")
        f.write(f"SEED_D={SEED_D}\n")
        f.write(f"{src}\n")
        f.write("z_msg=exp-mlkem-f203-kem-k4:SEED_Z=20260619\n")
        f.write(f"layout=dk_pke({DK_PKE_LEN})||ek({EK_LEN})||H({HASH_LEN})||z({HASH_LEN})\n")

    print(
        f"[gen_data] SEED_D={SEED_D} {src}; H/z/dk_kem golden ready; {note}",
        file=sys.stderr,
    )
    print(f"[gen_data] H_head={h[:4].hex()} z_head={z[:4].hex()}")


if __name__ == "__main__":
    main()
