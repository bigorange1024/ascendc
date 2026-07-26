#!/usr/bin/env python3
"""
gen_data_phase_e.py — Alg.21 Decaps device-k3 Phase-E-only 调试 fixture。

本脚本跳过 Phase-D Decrypt，直接令 m'=m，并按 D14 k3 host golden 生成合法 c、
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
D14_ENC = REPO / "ascendc-tests" / "ml-kem" / "ml-kem-768" / "pass-fix-f203-alg14-pke-encrypt-device-k3"
D19_KG = REPO / "ascendc-tests" / "ml-kem" / "ml-kem-768" / "pass-fix-f203-alg19-kem-keygen-device-k3"
HOST_GOLDEN = D14_ENC / "scripts" / "host_golden"
sys.path.insert(0, str(HOST_GOLDEN))

from f203_ref_common import K, N, Q, embed_message, load_lut_t_i8, stage123_transform  # noqa: E402
import golden_c as gc  # noqa: E402

_D19_GEN = D19_KG / "scripts" / "gen_data.py"
_spec = importlib.util.spec_from_file_location("mlkem768_d19_gen_data", _D19_GEN)
d19_gen_data = importlib.util.module_from_spec(_spec)
assert _spec.loader is not None
_spec.loader.exec_module(d19_gen_data)

EK_BYTES = 1184
DK_BYTES = 2400
CT_BYTES = 1088
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


def _load_or_generate_keypair() -> tuple[bytes, bytes, str]:
    """读取外部 k3 keypair，或默认按 D19 k3 oracle 从 SEED_D 生成。"""
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
        kem = d19_gen_data.build_kem_keygen(seed_d)
        ek = bytes(kem["ek"])
        dk = bytes(kem["dk_kem"])
        label = f"D19-local-seed-{seed_d}"
    if len(ek) != EK_BYTES or len(dk) != DK_BYTES:
        raise RuntimeError(f"bad k3 KEM key sizes ek={len(ek)} dk={len(dk)}")
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
    h = dk[2336:2368]
    z = dk[2368:2400]

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
    print(f"[gen_data_phase_e] dk←{key_label} c={len(c)}B K=32B via local k3 D14")


if __name__ == "__main__":
    main()
