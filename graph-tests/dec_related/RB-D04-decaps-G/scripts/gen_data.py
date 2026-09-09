#!/usr/bin/env python3
"""gen_data — RB-D04-decaps-G：m'[32] + h[32] → golden K'/r'（G=SHA3-512）。

本脚本是黑盒 oracle，不是 AscendC 实现规格。

h 契约（S0B / Decrypt KB）：
  dk_kem[3168] = dk_pke[1536] ‖ ek[1568] ‖ h[32] ‖ z[32]
  h ≡ dk_kem[3104:3136) 切片语义。
  本刀可直接喂 h.bin，但 **禁止** 用默认可重算的 H(ek) 冒充权威输入。
  生成流程：先构造假 dk_kem，再切片写出 h.bin（并落盘 dk_kem.bin 备查）。

权威：优先尝试 liboqs 同输入 G；不可用时回落 host hashlib.sha3_512（FEEDBACK 须标明）。
Host **不**把 golden K'/r' 喂给设备——仅写 output/golden_*.bin 供 verify。
"""
from __future__ import annotations

import hashlib
import os
import sys

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_CASE_DIR = os.path.normpath(os.path.join(_SCRIPT_DIR, ".."))

HALF = 32
DK_KEM_LEN = 3168
H_OFF = 3104  # dk_kem[3104:3136)
H_END = 3136

# 固定可复现向量（非全 0 m'）
FIXED_M_PRIME = bytes([(0x3C ^ (i * 17)) & 0xFF for i in range(HALF)])
# dk_kem 填充：使切片 h 非平凡，且与「随便 H(ek)」路径可区分
FIXED_DK_SEED = bytes([(0xA7 ^ (i * 13)) & 0xFF for i in range(DK_KEM_LEN)])


def g_sha3_512(m_prime: bytes, h: bytes) -> tuple[bytes, bytes]:
    """FIPS 203 G：(K' ‖ r') ← SHA3-512(m' ‖ h)。"""
    assert len(m_prime) == HALF and len(h) == HALF
    digest = hashlib.sha3_512(m_prime + h).digest()
    return digest[:HALF], digest[HALF:]


def try_liboqs_note() -> str:
    """探测 liboqs 是否可用；本刀 G 为纯 SHA3，liboqs 仅作「环境备注」。"""
    try:
        import oqs  # type: ignore  # noqa: F401

        return "liboqs_import_ok_but_G_via_hashlib"
    except Exception:
        pass
    # 仓内 liboqs 共享库探测（不调用 Decaps 实现）
    repo = os.path.normpath(os.path.join(_CASE_DIR, "../../.."))
    candidates = [
        os.path.join(repo, "thirdparty/liboqs/build/lib/liboqs.so"),
        os.path.join(repo, "thirdparty/liboqs/build/lib/liboqs.so.0"),
    ]
    for p in candidates:
        if os.path.isfile(p):
            return f"liboqs_so_present:{p};G_oracle=hashlib.sha3_512"
    return "G_oracle=hashlib.sha3_512 (no liboqs required for this knife)"


def main() -> None:
    os.makedirs(os.path.join(_CASE_DIR, "input"), exist_ok=True)
    os.makedirs(os.path.join(_CASE_DIR, "output"), exist_ok=True)

    dk = bytearray(FIXED_DK_SEED)
    # 显式写入切片区，强调「h 来自 dk 切片」而非 H(ek)
    h_slice = bytes([(0x5E ^ (i * 29)) & 0xFF for i in range(HALF)])
    dk[H_OFF:H_END] = h_slice
    h = bytes(dk[H_OFF:H_END])
    assert h == h_slice

    m_prime = FIXED_M_PRIME
    k_prime, r_prime = g_sha3_512(m_prime, h)
    oracle_note = try_liboqs_note()

    with open(os.path.join(_CASE_DIR, "input", "m_prime.bin"), "wb") as f:
        f.write(m_prime)
    with open(os.path.join(_CASE_DIR, "input", "h.bin"), "wb") as f:
        f.write(h)
    with open(os.path.join(_CASE_DIR, "input", "dk_kem.bin"), "wb") as f:
        f.write(bytes(dk))
    # 诊断：确认切片一致（设备不读 dk_kem.bin）
    with open(os.path.join(_CASE_DIR, "output", "golden_k_prime.bin"), "wb") as f:
        f.write(k_prime)
    with open(os.path.join(_CASE_DIR, "output", "golden_r_prime.bin"), "wb") as f:
        f.write(r_prime)
    with open(os.path.join(_CASE_DIR, "output", "oracle_note.txt"), "w", encoding="utf-8") as f:
        f.write(oracle_note + "\n")
        f.write("h_source=dk_kem[3104:3136)_slice\n")
        f.write("forbidden=default_H(ek)_as_authoritative_h\n")

    print(
        f"[gen_data] m'={HALF}B h=dk[{H_OFF}:{H_END}) K'/r'={HALF}B each; {oracle_note}",
        file=sys.stderr,
    )
    print(f"[gen_data] K'_head={k_prime[:4].hex()} r'_head={r_prime[:4].hex()}")


if __name__ == "__main__":
    main()
