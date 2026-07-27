#!/usr/bin/env python3
"""
gen_data.py — Alg.20 Encaps device-k2：ek + m + LUT + golden(c/K) + CPU golden_v。

流水线位置：
  1. 默认自生成 ML-KEM-512 ek_kem（800B），也允许 EK_KEM_SRC 覆盖；
  2. m 是 Alg.20 外部输入（M_HEX / M_FILE / 默认 urandom）；
  3. r = G(m‖H(ek)) 后半，仅作为 host golden 与 CPU 分段 golden_v 的参考，
     device 生产路径仍在 f203_kem_enc_prep 内自行计算 coins/r。

本脚本只提供 I/O oracle；禁止把 Python 计算过程当作 AscendC 实现规格。
"""
from __future__ import annotations

import hashlib
import os
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
REPO = _ascendc_repo_root(ROOT)
# 自包含：host_golden 已 vendored 到本目录（不再引用 ascendc-tests 探针）。
D14_ENC = ROOT
HOST_GOLDEN = D14_ENC / "scripts" / "host_golden"
sys.path.insert(0, str(HOST_GOLDEN))
sys.path.insert(0, str(REPO / "library" / "shared" / "fips203_host_rng"))

from f203_ref_common import K, N, Q, embed_message, load_lut_t_i8, stage123_transform  # noqa: E402
import golden_c as gc  # noqa: E402

SEED_D_DEFAULT = 20260619
EK_BYTES = 800
M_BYTES = 32
CT_BYTES = 768


def g_mh(m: bytes, ek: bytes) -> tuple[bytes, bytes]:
    """FIPS Alg.17 头：H(ek) 后再 G(m‖H(ek))，返回 K 与 r。"""
    h = hashlib.sha3_256(ek).digest()
    kr = hashlib.sha3_512(m + h).digest()
    return kr[:32], kr[32:]


def _gen_default_ek(inp: Path) -> bytes:
    """调用 D14 k2 host_golden 生成 ek_pke；Alg.20 中 ek_kem 与 ek_pke 同字节。"""
    import subprocess

    seed_d = int(os.environ.get("SEED_D", str(SEED_D_DEFAULT)))
    ek_path = inp / "ek_kem.bin"
    subprocess.check_call([sys.executable, str(HOST_GOLDEN / "gen_ek_pke.py"), str(seed_d), str(ek_path)], cwd=ROOT)
    ek = ek_path.read_bytes()
    if len(ek) != EK_BYTES:
        raise RuntimeError(f"bad generated ek size {len(ek)}")
    return ek


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

    if os.environ.get("EK_KEM_SRC"):
        ek_src = Path(os.environ["EK_KEM_SRC"])
        if not ek_src.is_file():
            print(f"[gen_data] missing ek: {ek_src}", file=sys.stderr)
            sys.exit(2)
        ek = ek_src.read_bytes()
        src_label = ek_src.name
    else:
        ek = _gen_default_ek(inp)
        src_label = f"local-seed-{os.environ.get('SEED_D', str(SEED_D_DEFAULT))}"
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

    k_ref, coins = g_mh(m, ek)
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

    c_ref = bytes(gc.golden_encrypt(ek, m, coins))
    if len(c_ref) != CT_BYTES:
        raise RuntimeError(f"bad c size {len(c_ref)}")
    (golden / "c.bin").write_bytes(c_ref)
    (golden / "K.bin").write_bytes(k_ref)
    print(f"[gen_data] ek←{src_label} m={m.hex()[:16]}… golden c={len(c_ref)}B K=32B via local D14")


if __name__ == "__main__":
    main()
