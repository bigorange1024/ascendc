#!/usr/bin/python3
# coding=utf-8
"""
EN15-encrypt-2launch · Alg.14 Host golden + 设备输入

权威交叉：liboqs PKE Encrypt（scripts/liboqs_pke_fixture.py）→ c[1568]。
本脚本：
  1) 生成/复用 fixture（ek_pke, m, coins, c）
  2) 派生设备输入：ρ、coins（Prep 种子）、t_hat、gammas、M4_ntt/M4_intt、e1/e2/μ
  3) Host 全量 Alg.14 自洽 golden（须先 c ≡ liboqs max=0，再接线设备）

Alg.14 段序（与 main 对齐）：
  L0 SampleNTT(ρ)→Â；Host 转置为 Âᵀ 喂 Matvec
  L1 Prep(coins)→y（nonce 0..3）；e1/e2 Host CBD（nonce 4..8）
  L2 NTT(y)→ŷ
  L3a Matvec(Âᵀ,ŷ)→û；L3b Dot(t̂,ŷ)→v̂
  L4 INTT(û)；Host INTT(v̂)；Host +e1/+e2+μ
  L5 Pack(u,v)→c
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
SEED_D = int(os.environ.get("SEED_D", "20260619"))
os.environ["FIPS203_PRF_BACKEND"] = "shake256"

XOF_BYTES = 672
CAND_PAIRS = XOF_BYTES // 3
C1_POLY_BYTES = n * 11 // 8  # 352
C2_BYTES = n * 5 // 8  # 160
C_BYTES = K * C1_POLY_BYTES + C2_BYTES  # 1568
EK_T_BYTES = 1536
UNIFIED_C = 41285357


def barrett_params(modulus: int):
    """Barrett 参数 b_k / b_mu，写入 tiling。"""
    b_k = 0
    temp_q = int(modulus)
    while temp_q > 0:
        temp_q >>= 1
        b_k += 1
    b_mu = (1 << (2 * b_k)) // int(modulus)
    return b_k, b_mu


def pack_m4_from_dense(m: np.ndarray) -> np.ndarray:
    """dense int32 → M4 四平面 int8。"""
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
    """γ_i = ζ^{2·BitRev7(i)+1} mod q。"""
    out = np.zeros(n // 2, dtype=np.int32)
    for i in range(n // 2):
        out[i] = pow(ZETA, 2 * bitrev7(i) + 1, q)
    return out


def mod_q(x: int) -> int:
    r = int(x) % q
    return r + q if r < 0 else r


def multiply_ntts(a: np.ndarray, b: np.ndarray, gammas: np.ndarray) -> np.ndarray:
    """Alg.11/12 paired basemul。"""
    c = np.zeros(n, dtype=np.int32)
    for i in range(n // 2):
        a0, a1 = int(a[2 * i]), int(a[2 * i + 1])
        b0, b1 = int(b[2 * i]), int(b[2 * i + 1])
        g = int(gammas[i])
        c[2 * i] = mod_q(a0 * b0 + a1 * b1 * g)
        c[2 * i + 1] = mod_q(a0 * b1 + a1 * b0)
    return c


def sample_ntt_poly(rho: bytes, j: int, i: int) -> np.ndarray:
    """Alg.7：SHAKE128(ρ‖j‖i).squeeze(672) → rej → â[256]。"""
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
    """Alg.6 ByteDecode₁₂：384B → int32[N]。"""
    assert len(buf) == 384
    out = np.zeros(n, dtype=np.int32)
    for i in range(n // 2):
        b0, b1, b2 = buf[3 * i], buf[3 * i + 1], buf[3 * i + 2]
        out[2 * i] = b0 | ((b1 & 0x0F) << 8)
        out[2 * i + 1] = (b1 >> 4) | (b2 << 4)
    return out


def decompress1_mu(m32: bytes) -> np.ndarray:
    """Decompress₁(ByteDecode₁(m))：bit→ (bit·q+1)//2。"""
    half_q = (q + 1) // 2
    mu = np.zeros(n, dtype=np.int32)
    for i in range(32):
        for j in range(8):
            bit = (m32[i] >> j) & 1
            mu[8 * i + j] = half_q * bit
    return mu


def compress_unified(u: int, d: int) -> int:
    """统一整数 Compress_d（与 enc_pack_compress_real / liboqs 等价）。"""
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
    """c = BE₁₁(Compress₁₁(u)) ‖ BE₅(Compress₅(v))。"""
    c1 = bytearray(K * C1_POLY_BYTES)
    u_mat = u.reshape(K, n)
    for p in range(K):
        comp = np.array([compress_unified(int(x), 11) for x in u_mat[p]], dtype=np.int32)
        c1[p * C1_POLY_BYTES : (p + 1) * C1_POLY_BYTES] = byte_encode_d11(comp)
    comp_v = np.array([compress_unified(int(x), 5) for x in v], dtype=np.int32)
    return bytes(c1) + byte_encode_d5(comp_v)


def ensure_liboqs_fixture(out_dir: Path) -> None:
    """调用仓内 fixture 脚本写入 ek/m/coins/c（权威）。"""
    sys.path.insert(0, str(_REPO / "scripts"))
    from liboqs_pke_fixture import generate_fixture  # noqa: WPS433

    generate_fixture(out_dir, SEED_D)


def gen_tiling() -> None:
    os.makedirs("input", exist_ok=True)
    os.makedirs("output", exist_ok=True)
    b_k, b_mu = barrett_params(q)
    r28 = (1 << 28) % int(q)
    print(f"[INFO] tiling: n={n}, bench={bench}, q={q}, b_k={b_k}, b_mu={b_mu}, r28={r28}")
    np.array([n, bench, q, b_k, b_mu, r28], dtype=np.int32).tofile("./input/tiling.bin")


def gen_encrypt_pipeline() -> None:
    """全量 Alg.14 造数 + 硬门禁：Host c ≡ liboqs c。"""
    if ref not in ("kyber", "ml-kem", "mlkem"):
        raise ValueError(f"EN13 锁定 NTT_REF=kyber，收到 {ref!r}")
    if bench != K:
        raise ValueError(f"EN13 要求 NTT_BENCH==K={K}，收到 bench={bench}")

    fix_dir = Path("./input/liboqs_fixture")
    ensure_liboqs_fixture(fix_dir)
    ek = (fix_dir / "ek_pke.bin").read_bytes()
    m = (fix_dir / "m.bin").read_bytes()
    coins = (fix_dir / "coins.bin").read_bytes()
    c_liboqs = (fix_dir / "c.bin").read_bytes()
    if len(ek) != C_BYTES or len(m) != 32 or len(coins) != 32 or len(c_liboqs) != C_BYTES:
        raise SystemExit("fixture size mismatch")

    # 设备/Host 共用输入镜像
    shutil.copyfile(fix_dir / "ek_pke.bin", "./input/ek_pke.bin")
    shutil.copyfile(fix_dir / "m.bin", "./input/m.bin")
    shutil.copyfile(fix_dir / "coins.bin", "./input/coins.bin")
    shutil.copyfile(fix_dir / "c.bin", "./input/c_liboqs.bin")
    # 权威 golden：设备最终须写出 output/c.bin 并与之 max=0
    shutil.copyfile(fix_dir / "c.bin", "./output/golden_c.bin")
    shutil.copyfile(fix_dir / "c.bin", "./output/c_liboqs.bin")

    rho = ek[EK_T_BYTES:C_BYTES]
    t_bytes = ek[:EK_T_BYTES]
    Path("./input/rho.bin").write_bytes(rho)
    # Prep 读「σ」槽，Encrypt 语义下为 coins/r
    Path("./input/coins.bin").write_bytes(coins)
    Path("./input/sigma.bin").write_bytes(coins)  # Prep 核仍叫 sigma

    # ---- t̂ = ByteDecode₁₂(t)（Host）----
    t_hat = np.zeros((K, n), dtype=np.int32)
    for i in range(K):
        t_hat[i] = byte_decode12(t_bytes[i * 384 : (i + 1) * 384])
    t_hat.reshape(-1).tofile("./input/t_hat.bin")
    print(f"[INFO] ByteDecode12 t_hat elems={t_hat.size}")

    # ---- Â = SampleNTT(ρ‖j‖i)；再 Âᵀ 扁平供 Matvec ----
    a_hat = np.zeros((K, K, n), dtype=np.int32)
    for i in range(K):
        for j in range(K):
            a_hat[i, j] = sample_ntt_poly(rho, j, i)
    a_flat = a_hat.reshape(-1)
    a_flat.tofile("./output/golden_a_hat.bin")
    # 转置：A_T[p,j]=A[j,p]，使既有 matvec(t[p]+=A[p,j]∘s[j]) 实现 Âᵀ∘ŷ
    a_t = np.transpose(a_hat, (1, 0, 2)).copy()
    a_t.reshape(-1).tofile("./output/golden_a_hat_T.bin")
    print(f"[INFO] SampleNTT Â + Host Âᵀ ready")

    # ---- CBD：y←PRF(r,0..3)；e1←4..7；e2←8 ----
    y_rows = [se.sample_poly_cbd2(se.prf_shake256(coins, i)) for i in range(K)]
    e1_rows = [se.sample_poly_cbd2(se.prf_shake256(coins, 4 + i)) for i in range(K)]
    e2 = se.sample_poly_cbd2(se.prf_shake256(coins, 8)).astype(np.int32)
    y = np.concatenate(y_rows).astype(np.int32)
    e1 = np.concatenate(e1_rows).astype(np.int32)
    mu = decompress1_mu(m)
    y.tofile("./output/golden_prep.bin")
    e1.tofile("./input/e1.bin")
    e2.tofile("./input/e2.bin")
    mu.tofile("./input/mu.bin")
    print(f"[INFO] CBD y/e1/e2 + μ embed Host-side")

    # ---- NTT(y) ----
    ntt_kyber.M = ntt_kyber.kyber_ntt_matrix(n=n, q=q)
    y_hat_list = [
        ntt_kyber.kyber_ntt(y[p * n : (p + 1) * n], n=n, q=q).astype(np.int32) for p in range(K)
    ]
    y_hat = np.concatenate(y_hat_list)
    y_hat_m = y_hat.reshape(K, n)
    pack_m4_from_dense(ntt_kyber.M).tofile("./input/M4_ntt.bin")
    y_hat.tofile("./output/golden_ntt.bin")

    # ---- û = Âᵀ ∘ ŷ；v̂ = ⟨t̂,ŷ⟩ ----
    gammas = gen_gammas()
    gammas.tofile("./input/gammas.bin")
    u_hat = np.zeros((K, n), dtype=np.int64)
    for j in range(K):
        for p in range(K):
            # A_T[p,j] = A[j,p]
            prod = multiply_ntts(a_hat[j, p], y_hat_m[j], gammas)
            u_hat[p] += prod.astype(np.int64)
    u_hat = np.mod(u_hat, q).astype(np.int32)
    v_hat_acc = np.zeros(n, dtype=np.int64)
    for i in range(K):
        v_hat_acc += multiply_ntts(t_hat[i], y_hat_m[i], gammas).astype(np.int64)
    v_hat = np.mod(v_hat_acc, q).astype(np.int32)
    u_hat.reshape(-1).tofile("./output/golden_matvec.bin")
    v_hat.tofile("./output/golden_dot.bin")
    # INTT(v̂) 走设备时：垫成 K poly（仅 poly0 有值）
    v_hat_pad = np.zeros((K, n), dtype=np.int32)
    v_hat_pad[0] = v_hat
    v_hat_pad.reshape(-1).tofile("./output/golden_v_hat_pad.bin")

    # ---- INTT + 噪声 ----
    u = np.concatenate(
        [
            np.asarray(inv.mlkem_inverse_ntt([int(x) for x in u_hat[p].tolist()]), dtype=np.int32)
            for p in range(K)
        ]
    )
    v = np.asarray(inv.mlkem_inverse_ntt([int(x) for x in v_hat.tolist()]), dtype=np.int32)
    m_inv = inv.build_mlkem_inverse_matrix_int32()
    pack_m4_from_dense(m_inv).tofile("./input/M4_intt.bin")
    u.tofile("./output/golden_intt_u.bin")
    v.tofile("./output/golden_intt_v.bin")

    u_noisy = np.mod(u.astype(np.int64) + e1.astype(np.int64), q).astype(np.int32)
    v_noisy = np.mod(
        v.astype(np.int64) + e2.astype(np.int64) + mu.astype(np.int64), q
    ).astype(np.int32)
    u_noisy.tofile("./output/golden_u_noisy.bin")
    v_noisy.tofile("./output/golden_v_noisy.bin")
    # 给 Pack 的 Host 侧对照（设备路径由 main 写回）
    u_noisy.tofile("./input/u_pack_expected.bin")
    v_noisy.tofile("./input/v_pack.bin")

    c_host = pack_ciphertext(u_noisy, v_noisy)
    Path("./output/golden_pack.bin").write_bytes(c_host)
    Path("./output/golden_c_host.bin").write_bytes(c_host)

    bad = sum(1 for a, b in zip(c_host, c_liboqs) if a != b)
    print(f"[GATE] Host Alg.14 c vs liboqs bad_bytes={bad}/{C_BYTES}")
    if bad != 0:
        for i, (a, b) in enumerate(zip(c_host, c_liboqs)):
            if a != b:
                print(f"[GATE] first diff @{i}: host={a:02x} liboqs={b:02x}")
                break
        raise SystemExit("EN13 Host golden c ≠ liboqs — 禁止继续接线设备")
    print("[OK] Host Alg.14 c ≡ liboqs max=0；设备输入已写")


if __name__ == "__main__":
    gen_tiling()
    gen_encrypt_pipeline()
    print("[OK] EN15 encrypt-2launch inputs written")
