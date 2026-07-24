#!/usr/bin/env python3
"""
gen_data.py — Alg.21 Decaps 全链输入。

默认（合法）：stash dk + liboqs encaps → dk_kem/c + golden K。
拒绝（KEM_DECAPS_REJECT=1）：随机 1568B 假密文 c；golden K = liboqs Decaps(dk,c)
  （≡ J(z‖c)）。liboqs 不暴露内部 c'；E3 对拍的是最终 K。
"""
from __future__ import annotations

import hashlib
import os
import subprocess
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
# exp-* → incubating → examples → repo
REPO = ROOT.parents[2]
# 本目录 vendored host_golden（禁止依赖其它 examples 路径）
HOST_GOLDEN = ROOT / "scripts" / "host_golden"
sys.path.insert(0, str(HOST_GOLDEN))
sys.path.insert(0, str(REPO / "library" / "shared" / "fips203_host_rng"))

from f203_ref_common import K, N, Q, embed_message, load_lut_t_i8, stage123_transform  # noqa: E402
import golden_c as gc  # noqa: E402

STASH = Path(os.environ.get("KEM_KEYPAIR_STASH", str(REPO / "output" / "kem_keypair_stash")))
EK_BYTES = 1568
DK_BYTES = 3168
CT_BYTES = 1568
M_BYTES = 32


def g_mh(m: bytes, h: bytes) -> tuple[bytes, bytes]:
    kr = hashlib.sha3_512(m + h).digest()
    return kr[:32], kr[32:]


def _j_zc(z: bytes, c: bytes) -> bytes:
    return hashlib.shake_256(z + c).digest(32)


def _ensure_liboqs_ref() -> Path:
    ref = REPO / "scripts" / "liboqs_kem_ref"
    if not ref.is_file():
        subprocess.check_call(["bash", str(REPO / "scripts" / "build_liboqs_kem_ref.sh")])
    return ref


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
    _lut_planar_stacked(lut_ntt, True).tofile(inp / "lut_even_stacked.bin")
    _lut_planar_stacked(lut_ntt, False).tofile(inp / "lut_odd_stacked.bin")


def _write_cpu_golden_v_placeholder(inp: Path, ek: bytes) -> None:
    """CPU pack 需要 v 缓冲；拒绝路径不要求 c' 正确，填零即可（仍几乎必 ≠ 假 c）。"""
    np.zeros((N,), dtype=np.int32).tofile(inp / "golden_v.bin")
    (inp / "coins.bin").write_bytes(bytes(32))


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
    ref = _ensure_liboqs_ref()

    reject = os.environ.get("KEM_DECAPS_REJECT", "0") == "1"
    (inp / "dk_kem.bin").write_bytes(dk)
    (inp / "ek_kem.bin").write_bytes(ek)
    (inp / "h.bin").write_bytes(h)
    (inp / "z.bin").write_bytes(z)
    _gen_luts(inp)

    if reject:
        # Gate E3：随机假密文（或 C_SRC）；golden = liboqs Decaps ≡ J(z‖c)
        if os.environ.get("C_SRC"):
            c = Path(os.environ["C_SRC"]).read_bytes()
        else:
            c = os.urandom(CT_BYTES)
        assert len(c) == CT_BYTES
        (inp / "c.bin").write_bytes(c)

        subprocess.check_call([str(ref), "decaps", str(dk_path), str(inp / "c.bin"), str(golden / "K.bin")])
        k_liboqs = (golden / "K.bin").read_bytes()
        k_j = _j_zc(z, c)
        if k_liboqs != k_j:
            print("[gen_data] BUG: liboqs Decaps(dk,c_bad) != J(z||c)", file=sys.stderr)
            sys.exit(3)
        (golden / "K_reject.bin").write_bytes(k_j)
        (golden / "mode_reject").write_text("1\n", encoding="utf-8")
        _write_cpu_golden_v_placeholder(inp, ek)
        print(f"[gen_data] REJECT c=urandom/C_SRC prefix={c[:8].hex()}… golden K=liboqs Decaps≡J(z||c)")
        return

    # --- 合法路径 ---
    # 若仓库级 roundtrip/KAT 传入 C_SRC，则必须原样喂给 device Decaps；
    # CPU Phase-E 仍需要该合法密文对应的 m 来生成 golden_v，因此调用方需传 M_FILE/M_HEX。
    if os.environ.get("M_FILE"):
        m = Path(os.environ["M_FILE"]).read_bytes()
    elif os.environ.get("M_HEX"):
        m = bytes.fromhex(os.environ["M_HEX"])
    else:
        m = os.urandom(M_BYTES)
    assert len(m) == M_BYTES

    if os.environ.get("C_SRC"):
        c = Path(os.environ["C_SRC"]).read_bytes()
        assert len(c) == CT_BYTES
        (inp / "c.bin").write_bytes(c)
        if os.environ.get("K_ENC_SRC"):
            (golden / "K.bin").write_bytes(Path(os.environ["K_ENC_SRC"]).read_bytes())
        else:
            # KAT 分项只需 device 输出；这里补 liboqs Decaps golden 便于 KEM_DECAPS_VERIFY=1 时自检。
            subprocess.check_call([str(ref), "decaps", str(dk_path), str(inp / "c.bin"), str(golden / "K.bin")])
    else:
        subprocess.check_call(
            [str(ref), "encaps", str(ek_path), str(inp / "c.bin"), str(golden / "K.bin"), m.hex()]
        )
    (inp / "m_prime_ref.bin").write_bytes(m)
    _k_ref, coins = g_mh(m, h)
    (inp / "coins.bin").write_bytes(coins)

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

    (golden / "mode_reject").unlink(missing_ok=True)
    print(f"[gen_data] full-chain dk←{dk_path.name} m={m.hex()[:16]}… golden K via liboqs encaps")


if __name__ == "__main__":
    main()
