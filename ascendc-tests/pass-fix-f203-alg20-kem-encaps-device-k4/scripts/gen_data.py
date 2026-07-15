#!/usr/bin/env python3
"""
gen_data.py — Alg.20 Encaps device-k4：ek + m + LUT + golden(c/K) + CPU golden_v。

m：外部 32B（M_HEX / M_FILE / 默认 urandom）。coins 仅作 Encrypt 参考 golden，不喂设备生产路径。
"""
from __future__ import annotations

import hashlib
import os
import shutil
import subprocess
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
REPO = ROOT.parents[1]
STABLE_ENC = REPO / "examples" / "stable" / "stable-fips203-mlkem-pke-encrypt-k4"
HOST_GOLDEN = STABLE_ENC / "scripts" / "host_golden"
sys.path.insert(0, str(HOST_GOLDEN))
sys.path.insert(0, str(REPO / "library" / "shared" / "fips203_host_rng"))

from f203_ref_common import K, N, Q, embed_message, load_lut_t_i8, stage123_transform  # noqa: E402
import golden_c as gc  # noqa: E402

EK_DEFAULT = (
    REPO / "ascendc-tests" / "pass-fix-f203-alg19-kem-keygen-device-k4" / "output" / "ek_kem.bin"
)
EK_BYTES = 1568
M_BYTES = 32


def g_mh(m: bytes, ek: bytes) -> tuple[bytes, bytes]:
    h = hashlib.sha3_256(ek).digest()
    kr = hashlib.sha3_512(m + h).digest()
    return kr[:32], kr[32:]


def _lut_planar_stacked(lut: np.ndarray, even: bool) -> np.ndarray:
    if even:
        top = lut[:, 0:N:2]
        bottom = lut[:, N:512:2]
    else:
        top = lut[:, 1:N:2]
        bottom = lut[:, N + 1 : 512 : 2]
    return np.concatenate([top, bottom], axis=0)


def _gen_luts(inp: Path) -> None:
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

    ek_src = Path(os.environ.get("EK_KEM_SRC", str(EK_DEFAULT)))
    if not ek_src.is_file():
        print(f"[gen_data] missing ek: {ek_src}", file=sys.stderr)
        sys.exit(2)
    ek = ek_src.read_bytes()
    assert len(ek) == EK_BYTES
    (inp / "ek_kem.bin").write_bytes(ek)
    (inp / "ek_pke.bin").write_bytes(ek)

    if os.environ.get("M_FILE"):
        m = Path(os.environ["M_FILE"]).read_bytes()
    elif os.environ.get("M_HEX"):
        m = bytes.fromhex(os.environ["M_HEX"])
    else:
        m = os.urandom(M_BYTES)
    assert len(m) == M_BYTES
    (inp / "m.bin").write_bytes(m)

    _k_ref, coins = g_mh(m, ek)
    (inp / "coins.bin").write_bytes(coins)  # CPU Encrypt 参考 / golden_v；device 自产 coins

    _gen_luts(inp)

    # CPU 辅助 golden_v（与 stable Encrypt gen_data._compute_golden_v 同式）
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

    ref = REPO / "scripts" / "liboqs_kem_ref"
    if not ref.is_file():
        subprocess.check_call(["bash", str(REPO / "scripts" / "build_liboqs_kem_ref.sh")])
    subprocess.check_call(
        [str(ref), "encaps", str(inp / "ek_kem.bin"), str(golden / "c.bin"), str(golden / "K.bin"), m.hex()]
    )
    print(f"[gen_data] ek←{ek_src.name} m={m.hex()[:16]}… golden c/K via liboqs")


if __name__ == "__main__":
    main()
