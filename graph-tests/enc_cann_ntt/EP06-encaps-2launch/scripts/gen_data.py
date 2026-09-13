#!/usr/bin/python3
# coding=utf-8
"""
EP06-encaps-2launch · Encaps 真调 EN15 形 Encrypt

Alg.16/20：m； (K̄,r)←G(m‖H(ek))；c←Encrypt(ek,m,r)。
设备段序与 EN15 同（SampleNTT…Pack）；coins 槽填 r。
"""

from __future__ import annotations

import hashlib
import os
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


def ensure_encaps_inputs(out_dir: Path) -> tuple[bytes, bytes, bytes, bytes, bytes]:
    """
    Encaps 造数：KeyGen→ek；m derand； (K̄,r)←G(m‖H(ek))；
    再用 liboqs/kem_ref Encaps 得权威 c/K（EP04 自洽门禁；EP04 再硬交叉）。
    @return ek, m, coins(r), c, K
    """
    sys.path.insert(0, str(_REPO / "scripts"))
    sys.path.insert(0, str(_REPO / "library" / "shared" / "f203_kem_ref"))
    from kem_ref import derand_d, derand_z, kem_keygen, kem_encaps, h_sha3_256, g_sha3_512
    from liboqs_kem_fixture import derand_m_from_seed

    out_dir.mkdir(parents=True, exist_ok=True)
    ek_path = out_dir / "ek_kem.bin"
    dk_path = out_dir / "dk_kem.bin"
    kem_seed = derand_d(SEED_D) + derand_z(SEED_D)
    src_kg = kem_keygen(kem_seed, ek_path, dk_path)
    ek = ek_path.read_bytes()
    m = derand_m_from_seed(SEED_D, 4)
    (out_dir / "m.bin").write_bytes(m)
    h = h_sha3_256(ek)
    k_bar, coins = g_sha3_512(m + h)
    (out_dir / "K_host.bin").write_bytes(k_bar)
    (out_dir / "r.bin").write_bytes(coins)
    c_path = out_dir / "c.bin"
    k_path = out_dir / "K.bin"
    src_ep = kem_encaps(ek, m, c_path, k_path, ek_path=ek_path)
    c = c_path.read_bytes()
    k = k_path.read_bytes()
    if k != k_bar:
        raise SystemExit("BUG: kem_encaps K ≠ Host G K̄")
    print(f"[INFO] encaps inputs via kg={src_kg} encaps={src_ep}")
    return ek, m, coins, c, k


def gen_tiling() -> None:
    os.makedirs("input", exist_ok=True)
    os.makedirs("output", exist_ok=True)
    b_k, b_mu = barrett_params(q)
    r28 = (1 << 28) % int(q)
    print(f"[INFO] tiling: n={n}, bench={bench}, q={q}, b_k={b_k}, b_mu={b_mu}, r28={r28}")
    np.array([n, bench, q, b_k, b_mu, r28], dtype=np.int32).tofile("./input/tiling.bin")


def gen_encrypt_pipeline() -> None:
    """Encaps→Encrypt：coins=r；Host Alg.14 c ≡ kem_encaps c。"""
    if ref not in ("kyber", "ml-kem", "mlkem"):
        raise ValueError(f"EP04 锁定 NTT_REF=kyber，收到 {ref!r}")
    if bench != K:
        raise ValueError(f"EP04 要求 NTT_BENCH==K={K}，收到 bench={bench}")

    fix_dir = Path("./input/encaps_fixture")
    ek, m, coins, c_ref, k_ref = ensure_encaps_inputs(fix_dir)
    if len(ek) != C_BYTES or len(m) != 32 or len(coins) != 32 or len(c_ref) != C_BYTES:
        raise SystemExit("encaps fixture size mismatch")

    Path("./input/ek_kem.bin").write_bytes(ek)
    Path("./input/m.bin").write_bytes(m)
    Path("./output/golden_c.bin").write_bytes(c_ref)
    Path("./output/golden_K.bin").write_bytes(k_ref)
    Path("./output/c_ref_encaps.bin").write_bytes(c_ref)

    rho = ek[EK_T_BYTES:C_BYTES]
    t_bytes = ek[:EK_T_BYTES]

    # ---- t̂ = ByteDecode₁₂(t)（Host）----
    t_hat = np.zeros((K, n), dtype=np.int32)
    for i in range(K):
        t_hat[i] = byte_decode12(t_bytes[i * 384 : (i + 1) * 384])
    print(f"[INFO] ByteDecode12 t_hat elems={t_hat.size}")

    # ---- Â = SampleNTT(ρ‖j‖i)；再 Âᵀ 扁平供 Matvec ----
    a_hat = np.zeros((K, K, n), dtype=np.int32)
    for i in range(K):
        for j in range(K):
            a_hat[i, j] = sample_ntt_poly(rho, j, i)
    a_flat = a_hat.reshape(-1)
    a_t = np.transpose(a_hat, (1, 0, 2)).copy()
    print(f"[INFO] SampleNTT Â ready (Âᵀ via matvec index; no Host transpose dump)")

    # ---- CBD：y←PRF(r,0..3)；e1←4..7；e2←8 ----
    y_rows = [se.sample_poly_cbd2(se.prf_shake256(coins, i)) for i in range(K)]
    e1_rows = [se.sample_poly_cbd2(se.prf_shake256(coins, 4 + i)) for i in range(K)]
    e2 = se.sample_poly_cbd2(se.prf_shake256(coins, 8)).astype(np.int32)
    y = np.concatenate(y_rows).astype(np.int32)
    e1 = np.concatenate(e1_rows).astype(np.int32)
    mu = decompress1_mu(m)
    print(f"[INFO] CBD y/e1/e2 + μ in memory for Host gate only (device regenerates)")

    # ---- NTT(y) ----
    ntt_kyber.M = ntt_kyber.kyber_ntt_matrix(n=n, q=q)
    y_hat_list = [
        ntt_kyber.kyber_ntt(y[p * n : (p + 1) * n], n=n, q=q).astype(np.int32) for p in range(K)
    ]
    y_hat = np.concatenate(y_hat_list)
    y_hat_m = y_hat.reshape(K, n)
    pack_m4_from_dense(ntt_kyber.M).tofile("./input/M4_ntt.bin")

    # ---- û = Âᵀ ∘ ŷ；v̂ = ⟨t̂,ŷ⟩ ----
    gammas = gen_gammas()
    gammas.tofile("./input/gammas.bin")
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
    v_hat_pad = np.zeros((K, n), dtype=np.int32)
    v_hat_pad[0] = v_hat

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

    u_noisy = np.mod(u.astype(np.int64) + e1.astype(np.int64), q).astype(np.int32)
    v_noisy = np.mod(
        v.astype(np.int64) + e2.astype(np.int64) + mu.astype(np.int64), q
    ).astype(np.int32)

    c_host = pack_ciphertext(u_noisy, v_noisy)

    bad = sum(1 for a, b in zip(c_host, c_ref) if a != b)
    print(f"[GATE] Host Alg.14 c vs encaps-ref bad_bytes={bad}/{C_BYTES}")
    if bad != 0:
        for i, (a, b) in enumerate(zip(c_host, c_ref)):
            if a != b:
                print(f"[GATE] first diff @{i}: host={a:02x} ref={b:02x}")
                break
        raise SystemExit("EP04 Host golden c ≠ encaps ref — 禁止接线设备")
    scrub_intermediate_bins()
    print("[OK] Host Encaps→Encrypt c ≡ ref；仅 ek_kem|m|LUT + golden_c/K")


def scrub_intermediate_bins() -> None:
    forbidden = [
        "rho.bin", "sigma.bin", "coins.bin", "t_hat.bin", "e1.bin", "e2.bin", "mu.bin",
        "u_pack_expected.bin", "v_pack.bin", "K_expected.bin",
    ]
    for name in forbidden:
        fp = Path("./input") / name
        if fp.is_file():
            fp.unlink()
            print(f"[CLEAN] removed input/{name}")
    for fp in Path("./output").glob("golden_*.bin"):
        if fp.name not in {"golden_c.bin", "golden_K.bin"}:
            fp.unlink()
            print(f"[CLEAN] removed {fp}")


if __name__ == "__main__":
    gen_tiling()
    gen_encrypt_pipeline()
    print("[OK] EP06 encaps-2launch inputs written")
