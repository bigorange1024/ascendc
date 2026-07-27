#!/usr/bin/env python3
"""
gen_data_phase_e.py — Alg.21 Decaps device-k2 Phase-E-only 调试 fixture。

本脚本跳过 Phase-D Decrypt，直接令 m'=m，并按 D14 k2 host golden 生成合法 c、
coins、golden_v 与 K。它只用于隔离 G+Encrypt+FO；正式验收默认跑 gen_data.py
全链 D+E。
"""
from __future__ import annotations

import hashlib
import importlib.util
import os
import sys
from pathlib import Path

import numpy as np


def _ascendc_repo_root(start: Path) -> Path:
    """自 start 向上查找含 AGENTS.md 与 scripts/ 的仓库根。"""
    p = start.resolve()
    for d in [p, *p.parents]:
        if (d / "AGENTS.md").is_file() and (d / "scripts").is_dir():
            return d
    raise RuntimeError(f"cannot locate ascendc repo root from {start}")


ROOT = Path(__file__).resolve().parent.parent
REPO = _ascendc_repo_root(ROOT)
D14_ENC = ROOT
D15_DEC = ROOT
HOST_GOLDEN = D14_ENC / "scripts" / "host_golden"
D15_HOST_GOLDEN = D15_DEC / "scripts" / "host_golden"
sys.path.insert(0, str(HOST_GOLDEN))
sys.path.insert(0, str(D15_HOST_GOLDEN))

from f203_ref_common import K, N, Q, embed_message, load_lut_t_i8, stage123_transform  # noqa: E402
import golden_c as gc  # noqa: E402

_D15_EK = D15_HOST_GOLDEN / "gen_ek_pke.py"
_ek_spec = importlib.util.spec_from_file_location("mlkem512_d15_gen_ek_pke", _D15_EK)
d15_gen_ek = importlib.util.module_from_spec(_ek_spec)
assert _ek_spec.loader is not None
_ek_spec.loader.exec_module(d15_gen_ek)

_D15_DK = D15_HOST_GOLDEN / "gen_dk_pke.py"
_dk_spec = importlib.util.spec_from_file_location("mlkem512_d15_gen_dk_pke", _D15_DK)
d15_gen_dk = importlib.util.module_from_spec(_dk_spec)
assert _dk_spec.loader is not None
_dk_spec.loader.exec_module(d15_gen_dk)

EK_BYTES = 800
DK_BYTES = 1632
CT_BYTES = 768
M_BYTES = 32
SEED_D_DEFAULT = 20260619


def g_mh(m: bytes, h: bytes) -> tuple[bytes, bytes]:
    """FIPS 203 G：SHA3-512(m‖h) → K'‖coins。"""
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
    """Phase-E 需要 Encrypt NTT/INTT 四套 LUT。"""
    lut_ntt = load_lut_t_i8("ntt")
    lut_intt = load_lut_t_i8("intt")
    _lut_planar_stacked(lut_ntt, True).tofile(inp / "lut_ntt_even_stacked.bin")
    _lut_planar_stacked(lut_ntt, False).tofile(inp / "lut_ntt_odd_stacked.bin")
    _lut_planar_stacked(lut_intt, True).tofile(inp / "lut_intt_even_stacked.bin")
    _lut_planar_stacked(lut_intt, False).tofile(inp / "lut_intt_odd_stacked.bin")


def _build_local_kem_keygen(seed_d: int) -> tuple[bytes, bytes]:
    """用活跃 D15/D14 k2 PKE host oracle 组装 Phase-E 调试用 KEM keypair。"""
    ek = bytes(d15_gen_ek.build_ek_pke(seed_d))
    dk_pke = bytes(d15_gen_dk.build_dk_pke(seed_d))
    h = hashlib.sha3_256(ek).digest()
    z = hashlib.sha3_256(f"ml-kem-512-d21-z:{seed_d}".encode()).digest()
    return ek, dk_pke + ek + h + z


def _load_or_generate_keypair() -> tuple[bytes, bytes, str]:
    """读取外部 k2 keypair，或默认按 D13 派生 fixture 从 SEED_D 生成。"""
    if os.environ.get("EK_KEM_SRC") or os.environ.get("DK_KEM_SRC"):
        ek_path = Path(os.environ.get("EK_KEM_SRC", ""))
        dk_path = Path(os.environ.get("DK_KEM_SRC", ""))
        if not ek_path.is_file() or not dk_path.is_file():
            print(f"[gen_data_phase_e] missing override ek/dk: {ek_path} {dk_path}", file=sys.stderr)
            sys.exit(2)
        ek = ek_path.read_bytes()
        dk = dk_path.read_bytes()
        label = f"override:{ek_path.name}/{dk_path.name}"
    else:
        seed_d = int(os.environ.get("SEED_D", str(SEED_D_DEFAULT)))
        ek, dk = _build_local_kem_keygen(seed_d)
        label = f"D15D14-local-seed-{seed_d}"
    if len(ek) != EK_BYTES or len(dk) != DK_BYTES:
        raise RuntimeError(f"bad k2 KEM key sizes ek={len(ek)} dk={len(dk)}")
    return ek, dk, label


def _write_golden_v(inp: Path, ek: bytes, m: bytes, coins: bytes) -> None:
    """写 CPU Phase-E 分段用 v=INTT(<t̂,r̂>)+μ+e₂。"""
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


def main() -> None:
    """写 Phase-E-only input/ 与 golden/K.bin。"""
    inp = ROOT / "input"
    golden = ROOT / "golden"
    inp.mkdir(parents=True, exist_ok=True)
    golden.mkdir(parents=True, exist_ok=True)
    (ROOT / "output").mkdir(parents=True, exist_ok=True)

    ek, dk, key_label = _load_or_generate_keypair()
    h = dk[1568:1600]
    z = dk[1600:1632]

    if os.environ.get("M_FILE"):
        m = Path(os.environ["M_FILE"]).read_bytes()
    elif os.environ.get("M_HEX"):
        m = bytes.fromhex(os.environ["M_HEX"])
    else:
        m = os.urandom(M_BYTES)
    if len(m) != M_BYTES:
        raise RuntimeError(f"bad m size {len(m)}")

    k_ref, coins = g_mh(m, h)
    c = bytes(gc.golden_encrypt(ek, m, coins))
    if len(c) != CT_BYTES:
        raise RuntimeError(f"bad c size {len(c)}")

    (inp / "ek_kem.bin").write_bytes(ek)
    (inp / "ek_pke.bin").write_bytes(ek)
    (inp / "m_prime.bin").write_bytes(m)
    (inp / "h.bin").write_bytes(h)
    (inp / "z.bin").write_bytes(z)
    (inp / "c.bin").write_bytes(c)
    (inp / "coins.bin").write_bytes(coins)
    _gen_luts(inp)
    _write_golden_v(inp, ek, m, coins)
    (golden / "K.bin").write_bytes(k_ref)
    print(f"[gen_data_phase_e] dk←{key_label} c={len(c)}B K=32B via local k2 D14")


if __name__ == "__main__":
    main()
