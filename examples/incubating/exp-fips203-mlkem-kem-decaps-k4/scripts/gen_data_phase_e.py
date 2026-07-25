#!/usr/bin/env python3
"""
gen_data_phase_e.py — **仅 Phase-E**（重加密 + FO）的 input / golden 生成（exp 自包含）。

## 与 gen_data.py 的分工
- gen_data.py：全链（含 Decrypt 段），支持 Gate E3 REJECT。
- 本脚本：假设 Decrypt 已得 m'=m，只生成 Phase-E 所需 ek、h、z、c、m'、coins、golden_v、golden/K。

## 合法路径语义
m' := encaps 所用 m（模拟正确 Decrypt）；设备 FO 应输出 encaps 的 K（与 golden/K.bin 一致）。

## golden_v
与 Encrypt 参考链相同：v = embed(INTT(tr̂), m) + e₂；供 CPU twin 在 pack 前对拍 v 系数。

## 路径解析（exp）
REPO = ROOT.parents[2]；HOST_GOLDEN = 本目录 scripts/host_golden（vendored）。
"""
from __future__ import annotations

import hashlib
import os
import subprocess
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
REPO = ROOT.parents[2]
HOST_GOLDEN = ROOT / "scripts" / "host_golden"
sys.path.insert(0, str(HOST_GOLDEN))
sys.path.insert(0, str(REPO / "library" / "shared" / "fips203_host_rng"))

from f203_ref_common import K, N, Q, embed_message, load_lut_t_i8, stage123_transform  # noqa: E402
import golden_c as gc  # noqa: E402

STASH = Path(os.environ.get("KEM_KEYPAIR_STASH", str(REPO / "output" / "kem_keypair_stash")))
EK_BYTES = 1568
DK_BYTES = 3168
M_BYTES = 32


def g_mh(m: bytes, h: bytes) -> tuple[bytes, bytes]:
    """G(m,h) → (K_ref, coins)；Phase-E 仅需 coins 段算 golden_v。"""
    kr = hashlib.sha3_512(m + h).digest()
    return kr[:32], kr[32:]


def _lut_planar_stacked(lut: np.ndarray, even: bool) -> np.ndarray:
    """LUT 平面堆叠（even/odd 列），写入 input/lut_*_stacked.bin。"""
    if even:
        top = lut[:, 0:N:2]
        bottom = lut[:, N:512:2]
    else:
        top = lut[:, 1:N:2]
        bottom = lut[:, N + 1 : 512 : 2]
    return np.concatenate([top, bottom], axis=0)


def _gen_luts(inp: Path) -> None:
    """Phase-E 仅需 NTT/INTT 四套 LUT。"""
    lut_ntt = load_lut_t_i8("ntt")
    lut_intt = load_lut_t_i8("intt")
    _lut_planar_stacked(lut_ntt, True).tofile(inp / "lut_ntt_even_stacked.bin")
    _lut_planar_stacked(lut_ntt, False).tofile(inp / "lut_ntt_odd_stacked.bin")
    _lut_planar_stacked(lut_intt, True).tofile(inp / "lut_intt_even_stacked.bin")
    _lut_planar_stacked(lut_intt, False).tofile(inp / "lut_intt_odd_stacked.bin")


def main() -> None:
    inp = ROOT / "input"
    golden = ROOT / "golden"
    inp.mkdir(parents=True, exist_ok=True)
    golden.mkdir(parents=True, exist_ok=True)
    (ROOT / "output").mkdir(parents=True, exist_ok=True)

    ek_path = Path(os.environ.get("EK_KEM_SRC", str(STASH / "ek_kem.bin")))
    dk_path = Path(os.environ.get("DK_KEM_SRC", str(STASH / "dk_kem.bin")))
    if not ek_path.is_file() or not dk_path.is_file():
        print(f"[gen_data] missing stash ek/dk: {ek_path} {dk_path}", file=sys.stderr)
        print("  run: bash scripts/kem_keypair_stash_bootstrap.sh", file=sys.stderr)
        sys.exit(2)

    ek = ek_path.read_bytes()
    dk = dk_path.read_bytes()
    assert len(ek) == EK_BYTES and len(dk) == DK_BYTES
    h = dk[3104:3136]
    z = dk[3136:3168]

    if os.environ.get("M_FILE"):
        m = Path(os.environ["M_FILE"]).read_bytes()
    elif os.environ.get("M_HEX"):
        m = bytes.fromhex(os.environ["M_HEX"])
    else:
        m = os.urandom(M_BYTES)
    assert len(m) == M_BYTES

    ref = REPO / "scripts" / "liboqs_kem_ref"
    if not ref.is_file():
        subprocess.check_call(["bash", str(REPO / "scripts" / "build_liboqs_kem_ref.sh")])
    subprocess.check_call(
        [str(ref), "encaps", str(ek_path), str(inp / "c.bin"), str(golden / "K.bin"), m.hex()]
    )

    (inp / "ek_kem.bin").write_bytes(ek)
    (inp / "ek_pke.bin").write_bytes(ek)
    (inp / "m_prime.bin").write_bytes(m)  # Phase-E：正确 Decrypt ⇒ m' = m
    (inp / "h.bin").write_bytes(h)
    (inp / "z.bin").write_bytes(z)

    _k_ref, coins = g_mh(m, h)
    (inp / "coins.bin").write_bytes(coins)  # 仅 golden_v 参考；设备核内自产噪声

    _gen_luts(inp)

    t_hat = gc.decode_t_hat(ek[: gc.EK_T_BYTES])
    r, _e1, e2 = gc.build_re(coins)
    r_hat = stage123_transform(r, "ntt")
    tr_hat = gc.golden_tr_hat(t_hat, r_hat)
    tr_pad = np.zeros((K, N), dtype=np.int32)
    tr_pad[0] = tr_hat
    tr = stage123_transform(tr_pad, "intt")[0]
    v = embed_message(tr, m)
    v = ((v.astype(np.int64) + e2.astype(np.int64)) % Q).astype(np.int32)
    v.tofile(inp / "golden_v.bin")

    print(f"[gen_data_phase_e] ek←{ek_path.name} m={m.hex()[:16]}… golden K via liboqs encaps")


if __name__ == "__main__":
    main()
