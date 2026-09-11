#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RB-T26 gen_data：liboqs KeyGen+Encaps → dk/c；golden_K ← liboqs Decaps。

契约：
  - 权威：scripts/liboqs_kem_ref（缺库 → BLOCKED，禁止 python 冒充）
  - Host 仅落盘 dk_kem[3168] + c[1568] + ζ/γ/mat；**不**预喂 K / m'
  - 禁抄 alg21/decaps 设备树造数逻辑
"""
from __future__ import annotations

import os
import sys
from pathlib import Path

import numpy as np

_SCRIPT = Path(__file__).resolve().parent
_CASE = _SCRIPT.parent
_REPO = _CASE.parents[2]

sys.path.insert(0, str(_SCRIPT))
sys.path.insert(0, str(_REPO / "library" / "shared" / "f203_kem_ref"))
from kem_ref import kem_decaps, kem_encaps, kem_keygen, liboqs_ref  # noqa: E402

# topology_math：优先本目录，否则借 T23/T10
for _cand in (
    _SCRIPT,
    _REPO / "graph-tests" / "enc_related" / "RB-T23-encaps-liboqs-cross" / "scripts",
    _REPO / "graph-tests" / "enc_related" / "RB-T10-uv-device-mix" / "scripts",
):
    if (_cand / "topology_math.py").is_file():
        sys.path.insert(0, str(_cand))
        break
from topology_math import load_zetas_gammas  # noqa: E402

FIXED_M = bytes([(0xA5 if (i % 2 == 0) else 0x5A) for i in range(32)])
FIXED_KEM_SEED = bytes([(0x11 + (i * 17)) & 0xFF for i in range(64)])
DK_KEM = 3168
C_LEN = 1568
K_LEN = 32


def _blocked(out: Path, msg: str) -> int:
    out.mkdir(parents=True, exist_ok=True)
    (out / "BLOCKED").write_text(msg + "\n", encoding="utf-8")
    (out / "cross_backend.txt").write_text("missing\n", encoding="utf-8")
    print(f"[BLOCKED] {msg}", file=sys.stderr)
    return 2


def main() -> int:
    inp = _CASE / "input"
    out = _CASE / "output"
    inp.mkdir(parents=True, exist_ok=True)
    out.mkdir(parents=True, exist_ok=True)
    for stale in (out / "BLOCKED",):
        if stale.is_file():
            stale.unlink()

    if liboqs_ref() is None:
        return _blocked(
            out,
            "缺 liboqs / scripts/liboqs_kem_ref；T26 权威 Decaps 不可用。"
            "请 bash scripts/clone-thirdparty.sh && bash scripts/build-liboqs.sh"
            " && bash scripts/build_liboqs_kem_ref.sh",
        )

    ek_path = out / "_liboqs_ek.bin"
    dk_path = out / "_liboqs_dk.bin"
    c_path = out / "_liboqs_c.bin"
    k_enc = out / "_liboqs_K_enc.bin"
    k_dec = out / "golden_K.bin"

    src_kg = kem_keygen(FIXED_KEM_SEED, ek_path, dk_path)
    if src_kg != "liboqs":
        return _blocked(out, f"kem_keygen 回落为 {src_kg!r}；T26 禁止非 liboqs 权威")
    dk = dk_path.read_bytes()
    ek = ek_path.read_bytes()
    if len(dk) != DK_KEM or len(ek) != C_LEN:
        return _blocked(out, f"keygen size dk={len(dk)} ek={len(ek)}")

    src_enc = kem_encaps(ek, FIXED_M, c_path, k_enc, ek_path=ek_path)
    if src_enc != "liboqs":
        return _blocked(out, f"kem_encaps 回落为 {src_enc!r}")
    c = c_path.read_bytes()
    if len(c) != C_LEN:
        return _blocked(out, f"encaps c size={len(c)}")

    src_dec = kem_decaps(dk, c, k_dec, dk_path=dk_path, c_path=c_path)
    if src_dec != "liboqs":
        return _blocked(out, f"kem_decaps 回落为 {src_dec!r}")
    golden_k = k_dec.read_bytes()
    if len(golden_k) != K_LEN:
        return _blocked(out, f"decaps K size={len(golden_k)}")
    if golden_k != k_enc.read_bytes():
        return _blocked(out, "liboqs Decaps(K) != Encaps(K)（fixture 坏）")

    (inp / "dk.bin").write_bytes(dk)
    (inp / "c.bin").write_bytes(c)
    (out / "cross_backend.txt").write_text("liboqs\n", encoding="utf-8")
    (out / "plain_m.bin").write_bytes(FIXED_M)  # 诊断用，非设备 input

    zetas_list, gammas_list = load_zetas_gammas()
    np.asarray(zetas_list, dtype=np.int32).tofile(inp / "zetas.bin")
    np.asarray(gammas_list, dtype=np.int32).tofile(inp / "gammas.bin")
    cube_rng = np.random.default_rng(20260908)
    cube_rng.integers(-8, 9, size=(16, 32), dtype=np.int8).tofile(inp / "mat_a.bin")
    cube_rng.integers(-8, 9, size=(32, 32), dtype=np.int8).tofile(inp / "mat_b.bin")

    print(
        f"[gen_data] dk={DK_KEM} c={C_LEN} golden_K via liboqs Decaps OK "
        f"SEED_ENV={os.environ.get('SEED_D', 'fixed')}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
