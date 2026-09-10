#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""gen_data — RB-D09 Decrypt 1-launch：dk_pke+c → golden_m。

权威优先：scripts/liboqs_pke_ref（PKE KeyGen+Encrypt+Decrypt）。
成功时写 cross_backend.txt=liboqs；缺库时回落 host FIPS 并写 host_fips（verify 将拒）。

输入：dk_pke.bin / c.bin / zetas.bin / gammas.bin / mat_a.bin / mat_b.bin
输出：golden_m.bin；禁预喂最终 m 到设备 input。
"""
from __future__ import annotations

import os
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np

_SCRIPT = Path(__file__).resolve().parent
_CASE = _SCRIPT.parent
_REPO = _CASE.parents[2]

sys.path.insert(0, str(_REPO / "graph-tests" / "enc_related" / "RB-T10-uv-device-mix" / "scripts"))
from topology_math import (  # noqa: E402
    Q,
    K,
    N,
    load_zetas_gammas,
    mlkem_ntt,
    mlkem_intt,
    multiply_ntts,
    mod_q,
)

SEED_D = int(os.environ.get("SEED_D", "20260909"))
DK_BYTES = 1536
C_BYTES = 1568
M_BYTES = 32
EK_BYTES = 1568
COINS_BYTES = 32
D_U = 11
D_V = 5
POLY_BYTES_12 = 384
POLY_BYTES_11 = 352
POLY_BYTES_5 = 160
COMPRESS_C = 41285357
COMPRESS1_SHIFT = 36
COMPRESS1_BIAS = 1 << 35

REF_BIN = _REPO / "scripts" / "liboqs_pke_ref"
BUILD_REF = _REPO / "scripts" / "build_liboqs_pke_ref_mlkem1024.sh"


def _try_liboqs_ref():
    """若本机有 liboqs_pke_ref 则返回路径；否则尝试 build，仍失败则 None。"""
    if REF_BIN.is_file() and os.access(REF_BIN, os.X_OK):
        return REF_BIN
    if not BUILD_REF.is_file():
        return None
    print(f"[gen_data] building {REF_BIN.name} via {BUILD_REF.name} …")
    try:
        subprocess.check_call(["bash", str(BUILD_REF)])
    except subprocess.CalledProcessError:
        return None
    if REF_BIN.is_file() and os.access(REF_BIN, os.X_OK):
        return REF_BIN
    return None


def _byte_encode12(coeffs: np.ndarray) -> np.ndarray:
    out = np.zeros(POLY_BYTES_12, dtype=np.uint8)
    for i in range(N // 2):
        v0 = int(coeffs[2 * i]) & 0xFFF
        v1 = int(coeffs[2 * i + 1]) & 0xFFF
        out[3 * i + 0] = v0 & 0xFF
        out[3 * i + 1] = ((v0 >> 8) & 0x0F) | ((v1 & 0x0F) << 4)
        out[3 * i + 2] = (v1 >> 4) & 0xFF
    return out


def _byte_encode11(coeffs: np.ndarray) -> np.ndarray:
    out = np.zeros(POLY_BYTES_11, dtype=np.uint8)
    for g in range(N // 8):
        t = [int(coeffs[g * 8 + j]) & 0x7FF for j in range(8)]
        b = g * 11
        out[b + 0] = t[0] & 0xFF
        out[b + 1] = ((t[0] >> 8) | (t[1] << 3)) & 0xFF
        out[b + 2] = ((t[1] >> 5) | (t[2] << 6)) & 0xFF
        out[b + 3] = (t[2] >> 2) & 0xFF
        out[b + 4] = ((t[2] >> 10) | (t[3] << 1)) & 0xFF
        out[b + 5] = ((t[3] >> 7) | (t[4] << 4)) & 0xFF
        out[b + 6] = ((t[4] >> 4) | (t[5] << 7)) & 0xFF
        out[b + 7] = (t[5] >> 1) & 0xFF
        out[b + 8] = ((t[5] >> 9) | (t[6] << 2)) & 0xFF
        out[b + 9] = ((t[6] >> 6) | (t[7] << 5)) & 0xFF
        out[b + 10] = (t[7] >> 3) & 0xFF
    return out


def _byte_encode5(coeffs: np.ndarray) -> np.ndarray:
    out = np.zeros(POLY_BYTES_5, dtype=np.uint8)
    for g in range(N // 8):
        t = [int(coeffs[g * 8 + j]) & 0x1F for j in range(8)]
        b = g * 5
        out[b + 0] = (t[0] | (t[1] << 5)) & 0xFF
        out[b + 1] = ((t[1] >> 3) | (t[2] << 2) | (t[3] << 7)) & 0xFF
        out[b + 2] = ((t[3] >> 1) | (t[4] << 4)) & 0xFF
        out[b + 3] = ((t[4] >> 4) | (t[5] << 1) | (t[6] << 6)) & 0xFF
        out[b + 4] = ((t[6] >> 2) | (t[7] << 3)) & 0xFF
    return out


def _decompress(comp: np.ndarray, d: int) -> np.ndarray:
    bias = 1 << (d - 1)
    return ((comp.astype(np.int64) * Q + bias) >> d).astype(np.int32)


def _compress1(u: int) -> int:
    s = COMPRESS_C * int(u) + COMPRESS1_BIAS
    return (s >> COMPRESS1_SHIFT) & 1


def _byte_encode1(bits: np.ndarray) -> bytes:
    out = bytearray(32)
    for i in range(N):
        out[i // 8] |= (int(bits[i]) & 1) << (i % 8)
    return bytes(out)


def _host_fips_full_chain(inp: Path, out: Path) -> bytes:
    """Host FIPS 自洽 oracle（非 liboqs）：仅作缺库回落；verify 要求 liboqs。"""
    rng = np.random.default_rng(SEED_D)

    s_hat = rng.integers(0, Q, size=(K, N), dtype=np.int32)
    u = rng.integers(0, Q, size=(K, N), dtype=np.int32)
    u_hat = np.stack([np.asarray(mlkem_ntt(u[i].tolist()), dtype=np.int32) for i in range(K)], 0)
    acc = np.zeros(N, dtype=np.int64)
    for j in range(K):
        prod = multiply_ntts(s_hat[j].tolist(), u_hat[j].tolist())
        acc += np.asarray(prod, dtype=np.int64)
    w_hat = np.array([mod_q(int(x)) for x in acc], dtype=np.int32)
    w = np.asarray(mlkem_intt(w_hat.tolist()), dtype=np.int32)

    m = bytes(rng.integers(0, 256, size=M_BYTES, dtype=np.uint8).tolist())
    bits = np.array([(m[i // 8] >> (i % 8)) & 1 for i in range(N)], dtype=np.int32)
    mu = ((bits.astype(np.int64) * Q + 1) >> 1).astype(np.int32)
    v = np.array([mod_q(int(w[i]) + int(mu[i])) for i in range(N)], dtype=np.int32)

    def compress_d(x: np.ndarray, d: int) -> np.ndarray:
        return ((((x.astype(np.int64) << d) + (Q // 2)) // Q) & ((1 << d) - 1)).astype(np.int32)

    u_comp = compress_d(u.reshape(-1), D_U).reshape(K, N)
    v_comp = compress_d(v, D_V)
    u_dec = _decompress(u_comp.reshape(-1), D_U).reshape(K, N)
    v_dec = _decompress(v_comp, D_V)

    u_hat2 = np.stack(
        [np.asarray(mlkem_ntt(u_dec[i].tolist()), dtype=np.int32) for i in range(K)], 0
    )
    acc2 = np.zeros(N, dtype=np.int64)
    for j in range(K):
        prod = multiply_ntts(s_hat[j].tolist(), u_hat2[j].tolist())
        acc2 += np.asarray(prod, dtype=np.int64)
    w_hat2 = np.array([mod_q(int(x)) for x in acc2], dtype=np.int32)
    w2 = np.asarray(mlkem_intt(w_hat2.tolist()), dtype=np.int32)
    bits2 = np.zeros(N, dtype=np.int32)
    for i in range(N):
        bits2[i] = _compress1(mod_q(int(v_dec[i]) - int(w2[i])))
    golden = _byte_encode1(bits2)

    dk = np.zeros(K * POLY_BYTES_12, dtype=np.uint8)
    for p in range(K):
        dk[p * POLY_BYTES_12 : (p + 1) * POLY_BYTES_12] = _byte_encode12(s_hat[p])
    c_u = np.zeros(K * POLY_BYTES_11, dtype=np.uint8)
    for p in range(K):
        c_u[p * POLY_BYTES_11 : (p + 1) * POLY_BYTES_11] = _byte_encode11(u_comp[p])
    c_v = _byte_encode5(v_comp)
    c = np.concatenate([c_u, c_v])

    (inp / "dk_pke.bin").write_bytes(bytes(dk))
    (inp / "c.bin").write_bytes(bytes(c))
    (out / "golden_m.bin").write_bytes(golden)
    (out / "plain_m.bin").write_bytes(m)
    return golden


def _liboqs_chain(inp: Path, out: Path, ref: Path) -> bytes:
    """liboqs PKE KeyGen+Encrypt → dk/c；golden ← Decrypt。"""
    rng = np.random.default_rng(SEED_D)
    d = bytes(rng.integers(0, 256, size=32, dtype=np.uint8).tolist())
    m = bytes(rng.integers(0, 256, size=M_BYTES, dtype=np.uint8).tolist())
    coins = bytes(rng.integers(0, 256, size=COINS_BYTES, dtype=np.uint8).tolist())

    with tempfile.TemporaryDirectory(prefix="rb_d09_") as td:
        tdp = Path(td)
        ek = tdp / "ek.bin"
        dk = tdp / "dk.bin"
        c = tdp / "c.bin"
        m_path = tdp / "m.bin"
        coins_path = tdp / "coins.bin"
        m_dec = tdp / "m_dec.bin"
        m_path.write_bytes(m)
        coins_path.write_bytes(coins)

        subprocess.check_call([str(ref), "keygen", str(ek), str(dk), d.hex()])
        if ek.stat().st_size != EK_BYTES or dk.stat().st_size != DK_BYTES:
            raise SystemExit(
                f"[BLOCKED] keygen size ek={ek.stat().st_size} dk={dk.stat().st_size}"
            )
        subprocess.check_call(
            [str(ref), "encrypt", str(c), str(ek), str(m_path), str(coins_path)]
        )
        if c.stat().st_size != C_BYTES:
            raise SystemExit(f"[BLOCKED] encrypt c size={c.stat().st_size}")
        subprocess.check_call([str(ref), "decrypt", str(m_dec), str(dk), str(c)])
        golden_m = m_dec.read_bytes()
        if len(golden_m) != M_BYTES:
            raise SystemExit(f"[BLOCKED] decrypt m size={len(golden_m)}")
        if golden_m != m:
            raise SystemExit("[BLOCKED] liboqs decrypt(m) != plaintext (fixture broken)")

        (inp / "dk_pke.bin").write_bytes(dk.read_bytes())
        (inp / "c.bin").write_bytes(c.read_bytes())
        (out / "golden_m.bin").write_bytes(golden_m)
        (out / "plain_m.bin").write_bytes(m)
        return golden_m


def main() -> int:
    inp = _CASE / "input"
    out = _CASE / "output"
    inp.mkdir(parents=True, exist_ok=True)
    out.mkdir(parents=True, exist_ok=True)

    ref = _try_liboqs_ref()
    if ref is not None:
        golden = _liboqs_chain(inp, out, ref)
        (out / "cross_backend.txt").write_text("liboqs\n", encoding="utf-8")
        backend = "liboqs"
    else:
        golden = _host_fips_full_chain(inp, out)
        (out / "cross_backend.txt").write_text("host_fips\n", encoding="utf-8")
        backend = "host_fips"
        print(
            "[gen_data] WARN: liboqs_pke_ref 不可用 → 回落 host FIPS；"
            "verify 要求 cross_backend=liboqs"
        )

    zetas_list, gammas_list = load_zetas_gammas()
    np.asarray(zetas_list, dtype=np.int32).tofile(inp / "zetas.bin")
    np.asarray(gammas_list, dtype=np.int32).tofile(inp / "gammas.bin")
    cube_rng = np.random.default_rng(SEED_D + 17)
    cube_rng.integers(-8, 9, size=(16, 32), dtype=np.int8).tofile(inp / "mat_a.bin")
    cube_rng.integers(-8, 9, size=(32, 32), dtype=np.int8).tofile(inp / "mat_b.bin")

    print(
        f"[gen_data] SEED_D={SEED_D} dk={DK_BYTES} c={C_BYTES} "
        f"golden_m via {backend} OK hex={golden.hex()[:16]}…"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
