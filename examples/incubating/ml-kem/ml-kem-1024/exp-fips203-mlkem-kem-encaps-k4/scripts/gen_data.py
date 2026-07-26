#!/usr/bin/env python3
"""
gen_data.py — Alg.20 Encaps incubating：ek + m + LUT + golden(c/K) + CPU golden_v。

FIPS：m 为输入；r 仅作 Host 参考派生（golden_v / 对照），不作为设备生产输入契约。
customspec：exp-fips203-mlkem-kem-encaps-k4-实现方案-customspec.*
"""
from __future__ import annotations

import hashlib
import os
import subprocess
import sys
from pathlib import Path

def _ascendc_repo_root(start: Path) -> Path:
    """自 start 向上查找含 AGENTS.md 与 scripts/ 的仓库根（兼容 ml-kem 参数组嵌套）。"""
    p = start.resolve()
    for d in [p, *p.parents]:
        if (d / "AGENTS.md").is_file() and (d / "scripts").is_dir():
            return d
    raise RuntimeError(f"cannot locate ascendc repo root from {start}")


import numpy as np

ROOT = Path(__file__).resolve().parent.parent
# exp-* → incubating → examples → repo
REPO = _ascendc_repo_root(ROOT)
HOST_GOLDEN = ROOT / "scripts" / "host_golden"
sys.path.insert(0, str(HOST_GOLDEN))
sys.path.insert(0, str(REPO / "library" / "shared" / "fips203_host_rng"))
sys.path.insert(0, str(REPO / "library" / "shared" / "fips203_se_sample"))

from f203_ref_common import K, N, Q, embed_message, load_lut_t_i8, stage123_transform  # noqa: E402
import golden_c as gc  # noqa: E402
from golden_se_sampling import derand_bytes_from_seed  # noqa: E402

EK_BYTES = 1568
M_BYTES = 32
SEED_D_DEFAULT = 20260619


def derand_z_from_seed(seed_d: int) -> bytes:
    """与 KEM KeyGen 设备 DerandZFromSeedD 同式。"""
    msg = f"exp-mlkem-f203-kem-k4:SEED_Z={seed_d}".encode()
    return hashlib.sha3_256(msg).digest()


def g_mh(m: bytes, ek: bytes) -> tuple[bytes, bytes]:
    """(K, r) ← G(m ‖ H(ek))，仅 golden / CPU 辅助。"""
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


def _ensure_liboqs_ref() -> Path:
    ref = REPO / "scripts" / "liboqs_kem_ref"
    if not ref.is_file():
        subprocess.check_call(["bash", str(REPO / "scripts" / "build_liboqs_kem_ref.sh")])
    return ref


def _bootstrap_ek(inp: Path, golden: Path) -> bytes:
    """缺省造 ek：liboqs keygen_derand(d‖z from SEED_D)；或 EK_KEM_SRC。"""
    ek_src = os.environ.get("EK_KEM_SRC")
    if ek_src:
        p = Path(ek_src)
        ek = p.read_bytes()
        assert len(ek) == EK_BYTES, f"ek size {len(ek)}"
        return ek
    existing = inp / "ek_kem.bin"
    if existing.is_file() and existing.stat().st_size == EK_BYTES and os.environ.get("KEEP_EK", "0") == "1":
        return existing.read_bytes()

    seed_d = int(os.environ.get("SEED_D", str(SEED_D_DEFAULT)))
    d = derand_bytes_from_seed(seed_d)
    z = derand_z_from_seed(seed_d)
    kem_seed = d + z
    ref = _ensure_liboqs_ref()
    ek_path = golden / "_bootstrap_ek_kem.bin"
    dk_path = golden / "_bootstrap_dk_kem.bin"
    subprocess.check_call([str(ref), "keygen", str(ek_path), str(dk_path), kem_seed.hex()])
    return ek_path.read_bytes()


def main() -> None:
    inp = ROOT / "input"
    golden = ROOT / "golden"
    inp.mkdir(parents=True, exist_ok=True)
    golden.mkdir(parents=True, exist_ok=True)
    (ROOT / "output").mkdir(parents=True, exist_ok=True)

    ek = _bootstrap_ek(inp, golden)
    (inp / "ek_kem.bin").write_bytes(ek)
    (inp / "ek_pke.bin").write_bytes(ek)

    if os.environ.get("M_FILE"):
        m = Path(os.environ["M_FILE"]).read_bytes()
    elif os.environ.get("M_HEX"):
        m = bytes.fromhex(os.environ["M_HEX"])
    else:
        # 默认定点 m，便于回归；可用 M_HEX / M_FILE / urandom 覆盖
        m = bytes.fromhex(os.environ.get("M_DEFAULT_HEX", "00" * 32))
        if os.environ.get("M_RANDOM", "0") == "1":
            m = os.urandom(M_BYTES)
    assert len(m) == M_BYTES
    (inp / "m.bin").write_bytes(m)

    _k_ref, r_ref = g_mh(m, ek)
    # 仅 golden 目录：供 CPU golden_v 派生；禁止作为设备生产读入契约
    (golden / "r_ref.bin").write_bytes(r_ref)

    _gen_luts(inp)

    # CPU 辅助 golden_v（tikicpu 分段无融合 INTT 时注入 v）
    t_hat = gc.decode_t_hat(ek[: gc.EK_T_BYTES])
    y, _e1, e2 = gc.build_re(r_ref)  # build_re(seed=r) → (y, e1, e2)
    y_hat = stage123_transform(y, "ntt")
    tr_hat = gc.golden_tr_hat(t_hat, y_hat)
    tr_pad = np.zeros((K, N), dtype=np.int32)
    tr_pad[0] = tr_hat
    tr = stage123_transform(tr_pad, "intt")[0]
    v = embed_message(tr, m)
    v = ((v.astype(np.int64) + e2.astype(np.int64)) % Q).astype(np.int32)
    v.tofile(inp / "golden_v.bin")

    ref = _ensure_liboqs_ref()
    subprocess.check_call(
        [str(ref), "encaps", str(inp / "ek_kem.bin"), str(golden / "c.bin"), str(golden / "K.bin"), m.hex()]
    )
    print(f"[gen_data] ek ready m={m.hex()[:16]}… golden c/K via liboqs encaps_derand")


if __name__ == "__main__":
    main()
