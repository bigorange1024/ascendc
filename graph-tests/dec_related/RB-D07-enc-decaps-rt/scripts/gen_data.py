#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""gen_data — RB-D07：设备 Encaps↔Decaps 往返输入 + liboqs 权威 golden。

本脚本是黑盒 oracle / 向量工厂，不是 AscendC 实现规格。

权威门禁：
  - KeyGen / Encaps / Decaps 一律 `scripts/liboqs_kem_ref`（liboqs）。
  - 缺库 → output/BLOCKED + exit 2（禁止 python Encaps/Decaps 冒充权威）。

Host 输入（禁预喂设备终产物 K/c/coins/c'）：
  m[32]、ek[1568]、dk_pke[1536]、h[32]=dk[3104:3136)、z[32]=dk[3136:3168)
  zetas/gammas/mat_a/mat_b（Encrypt+Decrypt Cube/NTT 共用）

golden：
  golden_k.bin ← liboqs Encaps(ek,m).K（≡ Decaps(dk,c).K）
  golden_c.bin ← liboqs Encaps 密文（仅诊断；主验收是 K 往返）
"""
from __future__ import annotations

import os
import sys
from pathlib import Path

import numpy as np

_SCRIPT = Path(__file__).resolve().parent
_CASE = _SCRIPT.parent
_REPO = _CASE.parents[2]

sys.path.insert(0, str(_REPO / "library" / "shared" / "f203_kem_ref"))
from kem_ref import kem_decaps, kem_encaps, kem_keygen, liboqs_ref  # noqa: E402

sys.path.insert(0, str(_REPO / "graph-tests" / "enc_related" / "RB-T10-uv-device-mix" / "scripts"))
from topology_math import load_zetas_gammas  # noqa: E402

HALF = 32
CT = 1568
DK_PKE = 1536
DK_KEM = 3168
H_OFF, H_END = 3104, 3136
Z_OFF, Z_END = 3136, 3168

FIXED_KEM_SEED = bytes([(0x29 + (i * 19)) & 0xFF for i in range(64)])
FIXED_M = bytes([(0xC3 ^ (i * 23)) & 0xFF for i in range(HALF)])


def _blocked(out: Path, msg: str) -> int:
    out.mkdir(parents=True, exist_ok=True)
    (out / "BLOCKED").write_text(msg + "\n", encoding="utf-8")
    (out / "cross_backend.txt").write_text("missing\n", encoding="utf-8")
    print(f"[BLOCKED] {msg}", file=sys.stderr)
    return 2


def main() -> int:
    out = _CASE / "output"
    inp = _CASE / "input"
    out.mkdir(parents=True, exist_ok=True)
    inp.mkdir(parents=True, exist_ok=True)

    if liboqs_ref() is None:
        return _blocked(
            out,
            "缺 liboqs / scripts/liboqs_kem_ref；K04 权威 Encaps/Decaps 不可用。"
            "请 bash scripts/clone-thirdparty.sh && bash scripts/build_liboqs_kem_ref.sh",
        )

    ek_path = out / "_ek_kem.bin"
    dk_path = out / "_dk_kem.bin"
    c_path = out / "golden_c.bin"
    k_path = out / "golden_k.bin"

    src_kg = kem_keygen(FIXED_KEM_SEED, ek_path, dk_path)
    if src_kg != "liboqs":
        return _blocked(out, f"kem_keygen 回落为 {src_kg!r}（须 liboqs）")

    ek = ek_path.read_bytes()
    dk = dk_path.read_bytes()
    if len(dk) != DK_KEM or len(ek) != CT:
        return _blocked(out, f"密钥尺寸异常 ek={len(ek)} dk={len(dk)}")

    src_enc = kem_encaps(ek, FIXED_M, c_path, k_path, ek_path=ek_path)
    if src_enc != "liboqs":
        return _blocked(out, f"kem_encaps 回落为 {src_enc!r}（须 liboqs）")

    c_ref = c_path.read_bytes()
    k_ref = k_path.read_bytes()
    assert len(c_ref) == CT and len(k_ref) == HALF

    # 权威 Decaps 闭合自检（fixture）
    k_dec_path = out / "_golden_k_decaps.bin"
    src_dec = kem_decaps(dk, c_ref, k_dec_path, dk_path=dk_path, c_path=c_path)
    if src_dec != "liboqs":
        return _blocked(out, f"kem_decaps 回落为 {src_dec!r}")
    if k_dec_path.read_bytes() != k_ref:
        return _blocked(out, "fixture 坏：liboqs Decaps(K) != Encaps(K)")

    # Host 输入：不写设备终产物 c/K/coins
    (inp / "m.bin").write_bytes(FIXED_M)
    (inp / "ek.bin").write_bytes(ek)
    (inp / "dk_pke.bin").write_bytes(dk[:DK_PKE])
    (inp / "h.bin").write_bytes(dk[H_OFF:H_END])
    (inp / "z.bin").write_bytes(dk[Z_OFF:Z_END])
    (inp / "dk_kem.bin").write_bytes(dk)  # 诊断 / 主控交叉备用

    zetas_list, gammas_list = load_zetas_gammas()
    np.asarray(zetas_list, dtype=np.int32).tofile(inp / "zetas.bin")
    np.asarray(gammas_list, dtype=np.int32).tofile(inp / "gammas.bin")
    cube_rng = np.random.default_rng(42)
    cube_rng.integers(-8, 9, size=(16, 32), dtype=np.int8).tofile(inp / "mat_a.bin")
    cube_rng.integers(-8, 9, size=(32, 32), dtype=np.int8).tofile(inp / "mat_b.bin")

    (out / "cross_backend.txt").write_text("liboqs\n", encoding="utf-8")
    (out / "oracle_note.txt").write_text(
        "authority=liboqs_kem_ref Encaps+Decaps\n"
        "accept=K_dec≡K_enc≡golden_k; golden_c diagnostic only\n"
        "h_source=dk_kem[3104:3136)_slice\n"
        "z_source=dk_kem[3136:3168)_slice\n"
        "host_no_prefeed=K,c,coins,c_prime\n"
        "forbidden=python_kem_as_authority\n",
        encoding="utf-8",
    )
    blocked = out / "BLOCKED"
    if blocked.is_file():
        blocked.unlink()

    print(
        f"[gen_data] ok backend=liboqs m={FIXED_M[:4].hex()}… "
        f"K={k_ref[:4].hex()}… c={c_ref[:4].hex()}…"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
