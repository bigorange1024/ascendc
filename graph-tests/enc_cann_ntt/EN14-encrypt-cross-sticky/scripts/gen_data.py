#!/usr/bin/python3
# coding=utf-8
"""
EN14-encrypt-cross-sticky · 粘性多轮 Encrypt×liboqs 造数

相对 EN13：同进程将跑 R 轮（默认 EN14_ROUNDS=8）；本脚本为每轮写独立输入：
  input/rXX/{rho,sigma,t_hat,e1,e2,mu}.bin + liboqs_fixture/
共享：tiling / gammas / M4_ntt / M4_intt（与轮无关）。

每轮 SEED_D_r = SEED_D + (r-1)*10007，换种子后 Host Alg.14 仍须 ≡ liboqs max=0。
"""

from __future__ import annotations

import hashlib
import os
import shutil
import sys
from pathlib import Path

import numpy as np

_REPO = Path(__file__).resolve().parents[4]
_SE = _REPO / "library" / "shared" / "fips203_se_sample"
sys.path.insert(0, str(_SE))
sys.path.insert(0, str(Path(__file__).resolve().parent))

import golden_se_sampling as se  # noqa: E402
import ntt_kyber  # noqa: E402
import gen_inverse_matrix as inv  # noqa: E402

n = int(os.environ.get("NTT_N", "256"))
bench = int(os.environ.get("NTT_BENCH", "4"))
q = int(os.environ.get("NTT_Q", "3329"))
ref = os.environ.get("NTT_REF", "kyber").lower()
K = 4
ZETA = 17
SEED_D0 = int(os.environ.get("SEED_D", "20260619"))
ROUNDS = int(os.environ.get("EN14_ROUNDS", "8"))
os.environ["FIPS203_PRF_BACKEND"] = "shake256"

XOF_BYTES = 672
CAND_PAIRS = XOF_BYTES // 3
C1_POLY_BYTES = n * 11 // 8
C2_BYTES = n * 5 // 8
C_BYTES = K * C1_POLY_BYTES + C2_BYTES
EK_T_BYTES = 1536
UNIFIED_C = 41285357


def barrett_params(modulus: int):
    b_k = 0
    temp_q = int(modulus)
    while temp_q > 0:
        temp_q >>= 1
        b_k += 1
    b_mu = (1 << (2 * b_k)) // int(modulus)
    return b_k, b_mu


def pack_m4_from_dense(m: np.ndarray) -> np.ndarray:
    m = m.astype(np.int32)
    m0 = ((m >> 0) & 0x7F).astype(np.int8).reshape(-1)
    m1 = ((m >> 7) & 0x7F).astype(np.int8).reshape(-1)
    m2 = ((m >> 14) & 0x7F).astype(np.int8).reshape(-1)
    m3 = ((m >> 21) & 0x7F).astype(np.int8).reshape(-1)
    return np.concatenate((m0, m1, m2, m3), dtype=np.int8)


def bitrev7(i: int) -> int:
    r = 0
    for b in range(7):
        r = (r << 1) | ((i >> b) & 1)
    return r


def gen_gammas() -> np.ndarray:
    out = np.zeros(n // 2, dtype=np.int32)
    for i in range(n // 2):
        out[i] = pow(ZETA, 2 * bitrev7(i) + 1, q)
    return out


def mod_q(x: int) -> int:
    r = int(x) % q
    return r + q if r < 0 else r


def multiply_ntts(a: np.ndarray, b: np.ndarray, gammas: np.ndarray) -> np.ndarray:
    c = np.zeros(n, dtype=np.int32)
    for i in range(n // 2):
        a0, a1 = int(a[2 * i]), int(a[2 * i + 1])
        b0, b1 = int(b[2 * i]), int(b[2 * i + 1])
        g = int(gammas[i])
        c[2 * i] = mod_q(a0 * b0 + a1 * b1 * g)
        c[2 * i + 1] = mod_q(a0 * b1 + a1 * b0)
    return c


def sample_ntt_poly(rho: bytes, j: int, i: int) -> np.ndarray:
    msg = rho + bytes([j & 0xFF, i & 0xFF])
    buf = hashlib.shake_128(msg).digest(XOF_BYTES)
    out: list[int] = []
    pos = 0
    for _t in range(CAND_PAIRS):
        c0, c1, c2 = buf[pos], buf[pos + 1], buf[pos + 2]
        pos += 3
        d1 = c0 + 256 * (c1 & 0x0F)
        d2 = (c1 >> 4) + 16 * c2
        if d1 < q and len(out) < n:
            out.append(d1)
        if d2 < q and len(out) < n:
            out.append(d2)
        if len(out) >= n:
            break
    if len(out) < n:
        raise SystemExit(f"SampleNTT rej short: got {len(out)}")
    return np.array(out[:n], dtype=np.int32)


def byte_decode12(buf: bytes) -> np.ndarray:
    assert len(buf) == 384
    out = np.zeros(n, dtype=np.int32)
    for i in range(n // 2):
        b0, b1, b2 = buf[3 * i], buf[3 * i + 1], buf[3 * i + 2]
        out[2 * i] = b0 | ((b1 & 0x0F) << 8)
        out[2 * i + 1] = (b1 >> 4) | (b2 << 4)
    return out


def decompress1_mu(m32: bytes) -> np.ndarray:
    half_q = (q + 1) // 2
    mu = np.zeros(n, dtype=np.int32)
    for i in range(32):
        for j in range(8):
            bit = (m32[i] >> j) & 1
            mu[8 * i + j] = half_q * bit
    return mu


def compress_unified(u: int, d: int) -> int:
    u = int(u) % q
    if u < 0:
        u += q
    y = (UNIFIED_C * u + (1 << (36 - d))) >> (37 - d)
    return int(y) & ((1 << d) - 1)


def byte_encode_d5(comp: np.ndarray) -> bytes:
    out = bytearray(C2_BYTES)
    for i in range(n // 8):
        t = [int(comp[8 * i + j]) & 0x1F for j in range(8)]
        base = i * 5
        out[base + 0] = 0xFF & ((t[0] >> 0) | (t[1] << 5))
        out[base + 1] = 0xFF & ((t[1] >> 3) | (t[2] << 2) | (t[3] << 7))
        out[base + 2] = 0xFF & ((t[3] >> 1) | (t[4] << 4))
        out[base + 3] = 0xFF & ((t[4] >> 4) | (t[5] << 1) | (t[6] << 6))
        out[base + 4] = 0xFF & ((t[6] >> 2) | (t[7] << 3))
    return bytes(out)


def byte_encode_d11(comp: np.ndarray) -> bytes:
    out = bytearray(C1_POLY_BYTES)
    for j in range(n // 8):
        t = [int(comp[8 * j + k]) & 0x7FF for k in range(8)]
        base = j * 11
        out[base + 0] = (t[0] >> 0) & 0xFF
        out[base + 1] = ((t[0] >> 8) | ((t[1] << 3) & 0xFF)) & 0xFF
        out[base + 2] = ((t[1] >> 5) | ((t[2] << 6) & 0xFF)) & 0xFF
        out[base + 3] = (t[2] >> 2) & 0xFF
        out[base + 4] = ((t[2] >> 10) | ((t[3] << 1) & 0xFF)) & 0xFF
        out[base + 5] = ((t[3] >> 7) | ((t[4] << 4) & 0xFF)) & 0xFF
        out[base + 6] = ((t[4] >> 4) | ((t[5] << 7) & 0xFF)) & 0xFF
        out[base + 7] = (t[5] >> 1) & 0xFF
        out[base + 8] = ((t[5] >> 9) | ((t[6] << 2) & 0xFF)) & 0xFF
        out[base + 9] = ((t[6] >> 6) | ((t[7] << 5) & 0xFF)) & 0xFF
        out[base + 10] = (t[7] >> 3) & 0xFF
    return bytes(out)


def pack_ciphertext(u: np.ndarray, v: np.ndarray) -> bytes:
    c1 = bytearray(K * C1_POLY_BYTES)
    u_mat = u.reshape(K, n)
    for p in range(K):
        comp = np.array([compress_unified(int(x), 11) for x in u_mat[p]], dtype=np.int32)
        c1[p * C1_POLY_BYTES : (p + 1) * C1_POLY_BYTES] = byte_encode_d11(comp)
    comp_v = np.array([compress_unified(int(x), 5) for x in v], dtype=np.int32)
    return bytes(c1) + byte_encode_d5(comp_v)


def ensure_liboqs_fixture(out_dir: Path, seed_d: int) -> None:
    sys.path.insert(0, str(_REPO / "scripts"))
    from liboqs_pke_fixture import generate_fixture  # noqa: WPS433

    generate_fixture(out_dir, seed_d)


def gen_tiling_and_shared() -> None:
    """写共享 tiling / gammas / M4（一轮即可）。"""
    os.makedirs("input", exist_ok=True)
    os.makedirs("output", exist_ok=True)
    b_k, b_mu = barrett_params(q)
    r28 = (1 << 28) % int(q)
    print(f"[INFO] tiling: n={n}, bench={bench}, q={q}, R={ROUNDS}")
    np.array([n, bench, q, b_k, b_mu, r28], dtype=np.int32).tofile("./input/tiling.bin")
    Path("./input/rounds.txt").write_text(f"{ROUNDS}\n", encoding="utf-8")

    gammas = gen_gammas()
    gammas.tofile("./input/gammas.bin")
    ntt_kyber.M = ntt_kyber.kyber_ntt_matrix(n=n, q=q)
    pack_m4_from_dense(ntt_kyber.M).tofile("./input/M4_ntt.bin")
    m_inv = inv.build_mlkem_inverse_matrix_int32()
    pack_m4_from_dense(m_inv).tofile("./input/M4_intt.bin")
    print("[INFO] shared gammas/M4_ntt/M4_intt written")


def gen_one_round(round_idx: int) -> None:
    """为第 round_idx 轮（1-based）写设备输入 + Host/liboqs 门禁。"""
    if ref not in ("kyber", "ml-kem", "mlkem"):
        raise ValueError(f"EN14 锁定 NTT_REF=kyber，收到 {ref!r}")
    if bench != K:
        raise ValueError(f"EN14 要求 NTT_BENCH==K={K}，收到 bench={bench}")

    seed_d = SEED_D0 + (round_idx - 1) * 10007
    rdir = Path(f"./input/r{round_idx:02d}")
    rdir.mkdir(parents=True, exist_ok=True)
    fix_dir = rdir / "liboqs_fixture"
    ensure_liboqs_fixture(fix_dir, seed_d)

    ek = (fix_dir / "ek_pke.bin").read_bytes()
    m = (fix_dir / "m.bin").read_bytes()
    coins = (fix_dir / "coins.bin").read_bytes()
    c_liboqs = (fix_dir / "c.bin").read_bytes()
    if len(ek) != C_BYTES or len(m) != 32 or len(coins) != 32 or len(c_liboqs) != C_BYTES:
        raise SystemExit(f"r{round_idx:02d} fixture size mismatch")

    rho = ek[EK_T_BYTES:C_BYTES]
    t_bytes = ek[:EK_T_BYTES]
    (rdir / "rho.bin").write_bytes(rho)
    (rdir / "coins.bin").write_bytes(coins)
    (rdir / "sigma.bin").write_bytes(coins)
    (rdir / "m.bin").write_bytes(m)
    (rdir / "seed_d.txt").write_text(f"{seed_d}\n", encoding="utf-8")

    t_hat = np.zeros((K, n), dtype=np.int32)
    for i in range(K):
        t_hat[i] = byte_decode12(t_bytes[i * 384 : (i + 1) * 384])
    t_hat.reshape(-1).tofile(rdir / "t_hat.bin")

    a_hat = np.zeros((K, K, n), dtype=np.int32)
    for i in range(K):
        for j in range(K):
            a_hat[i, j] = sample_ntt_poly(rho, j, i)

    y_rows = [se.sample_poly_cbd2(se.prf_shake256(coins, i)) for i in range(K)]
    e1_rows = [se.sample_poly_cbd2(se.prf_shake256(coins, 4 + i)) for i in range(K)]
    e2 = se.sample_poly_cbd2(se.prf_shake256(coins, 8)).astype(np.int32)
    y = np.concatenate(y_rows).astype(np.int32)
    e1 = np.concatenate(e1_rows).astype(np.int32)
    mu = decompress1_mu(m)
    e1.tofile(rdir / "e1.bin")
    e2.tofile(rdir / "e2.bin")
    mu.tofile(rdir / "mu.bin")

    # Host Alg.14 自洽门禁（与 EN13 同式；须先绿再接线）
    ntt_kyber.M = ntt_kyber.kyber_ntt_matrix(n=n, q=q)
    y_hat_list = [
        ntt_kyber.kyber_ntt(y[p * n : (p + 1) * n], n=n, q=q).astype(np.int32) for p in range(K)
    ]
    y_hat = np.concatenate(y_hat_list)
    y_hat_m = y_hat.reshape(K, n)
    gammas = gen_gammas()
    u_hat = np.zeros((K, n), dtype=np.int64)
    for j in range(K):
        for p in range(K):
            prod = multiply_ntts(a_hat[j, p], y_hat_m[j], gammas)
            u_hat[p] += prod.astype(np.int64)
    u_hat = np.mod(u_hat, q).astype(np.int32)
    v_hat_acc = np.zeros(n, dtype=np.int64)
    for i in range(K):
        v_hat_acc += multiply_ntts(t_hat[i], y_hat_m[i], gammas).astype(np.int64)
    v_hat = np.mod(v_hat_acc, q).astype(np.int32)
    u = np.concatenate(
        [
            np.asarray(inv.mlkem_inverse_ntt([int(x) for x in u_hat[p].tolist()]), dtype=np.int32)
            for p in range(K)
        ]
    )
    v = np.asarray(inv.mlkem_inverse_ntt([int(x) for x in v_hat.tolist()]), dtype=np.int32)
    u_noisy = np.mod(u.astype(np.int64) + e1.astype(np.int64), q).astype(np.int32)
    v_noisy = np.mod(
        v.astype(np.int64) + e2.astype(np.int64) + mu.astype(np.int64), q
    ).astype(np.int32)
    c_host = pack_ciphertext(u_noisy, v_noisy)
    bad = sum(1 for a, b in zip(c_host, c_liboqs) if a != b)
    print(f"[GATE] r{round_idx:02d} seed={seed_d} Host c vs liboqs bad={bad}/{C_BYTES}")
    if bad != 0:
        raise SystemExit(f"EN14 r{round_idx:02d} Host golden c ≠ liboqs — 禁止接线")
    # 轮 1 额外落一份到顶层，便于手工对照
    if round_idx == 1:
        shutil.copyfile(fix_dir / "c.bin", "./output/c_liboqs_r01.bin")
        Path("./output/golden_c_host_r01.bin").write_bytes(c_host)
    print(f"[OK] r{round_idx:02d} Host≡liboqs；输入已写 {rdir}")


if __name__ == "__main__":
    if ROUNDS < 1 or ROUNDS > 64:
        raise SystemExit(f"EN14_ROUNDS={ROUNDS} invalid")
    gen_tiling_and_shared()
    for r in range(1, ROUNDS + 1):
        gen_one_round(r)
    print(f"[OK] EN14 sticky inputs written R={ROUNDS}")
