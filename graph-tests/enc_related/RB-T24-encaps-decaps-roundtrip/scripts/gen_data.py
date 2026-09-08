#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RB-T24 gen_data：T23 设备 Encaps 路径 + liboqs KeyGen(sk) 供 Decaps 往返。

权威（本刀硬门禁）：
  1) liboqs KeyGen → 合法 (ek, sk)；sk 写入 output/sk.bin（verify 用 Decaps）
  2) 同 m/ek 调 liboqs Encaps → golden_c / golden_K（诊断交叉，非往返主门）
  缺库 → 写 output/BLOCKED 并 exit 2（勿假绿；禁止 python 冒充权威）

设备路径（同 T23；仅 Host 复算中间量便于诊断）：
  h←H(ek)；(K‖r)←G(m‖h)；coins=r；μ←m；Decode₁₂→CBD→Â/ŷ→u,v→c
  禁写 coins/K/h/最终 μ/t̂/y/e/Â/ŷ/u/v/c 到 input
"""
from __future__ import annotations

import hashlib
import sys
from pathlib import Path

import numpy as np

_SCRIPT = Path(__file__).resolve().parent
_CASE = _SCRIPT.parent
_REPO = _CASE.parents[2]

sys.path.insert(0, str(_SCRIPT))
sys.path.insert(0, str(_REPO / "graph-tests" / "enc_related" / "RB-T07-prep-shell" / "scripts"))
sys.path.insert(0, str(_REPO / "library" / "shared" / "fips203_se_sample"))
sys.path.insert(0, str(_REPO / "library" / "shared" / "f203_kem_ref"))
from prep_host import extract_rho  # noqa: E402
from golden_se_sampling import sample_poly_cbd2  # noqa: E402
from kem_ref import (  # noqa: E402
    kem_encaps,
    kem_keygen,
    liboqs_ref,
)
from topology_math import (  # noqa: E402
    K,
    N,
    Q,
    compute_u_v,
    load_zetas_gammas,
    mlkem_ntt,
    mu_embed_from_m,
)

ALG7_SCRIPTS = (
    _REPO
    / "ascendc-tests"
    / "ml-kem"
    / "ml-kem-1024"
    / "pass-fix-f203-alg7-sample-ntt-k4"
    / "scripts"
)
sys.path.insert(0, str(ALG7_SCRIPTS))
from alg7_geom import XOF_BYTES  # noqa: E402
from gen_data import (  # noqa: E402
    rej_bulk_from_d12,
    rej_scalar_from_d12,
    shake128_squeeze,
    unpack_d12_from_xof,
)

# 固定种子：可复现；m 非全 0（Encaps 禁默认可全 0）
SEED_MAT = 42
FIXED_M = bytes([(0xA5 if (i % 2 == 0) else 0x5A) for i in range(32)])
# kem_seed = d‖z（64B）；仅用于 liboqs KeyGen derand
FIXED_KEM_SEED = bytes([(0x11 + (i * 17)) & 0xFF for i in range(64)])
C_LEN = 1568
AHAT_POLYS = K * K
PRF_OUT = 2 * N // 4  # 128
POLY_BYTES = 384  # ByteEncode₁₂
K_LEN = 32
COINS_LEN = 32


def _blocked(out: Path, msg: str) -> int:
    """缺权威库：写 BLOCKED 标记并返回 2（run.sh / FEEDBACK 认）。"""
    out.mkdir(parents=True, exist_ok=True)
    (out / "BLOCKED").write_text(msg + "\n", encoding="utf-8")
    (out / "cross_backend.txt").write_text("missing\n", encoding="utf-8")
    print(f"[BLOCKED] {msg}", file=sys.stderr)
    return 2


def encaps_head(m: bytes, ek: bytes) -> tuple[bytes, bytes, bytes]:
    """Alg.17 外形：h=H(ek)，(K‖r)=G(m‖h)。返回 (K, r/coins, h)。"""
    assert len(m) == 32 and len(ek) == C_LEN
    h = hashlib.sha3_256(ek).digest()
    g = hashlib.sha3_512(m + h).digest()
    assert len(g) == 64
    return g[:K_LEN], g[K_LEN:], h


def prf_shake256(coins: bytes, nonce: int) -> bytes:
    return hashlib.shake_256(coins + bytes([nonce & 0xFF])).digest(PRF_OUT)


def sample_y_e1_e2(coins: bytes) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """Alg.14 行 8–15：coins→y/e1/e2（η=2）。"""
    nonce = 0
    y_rows = []
    for _ in range(K):
        y_rows.append(sample_poly_cbd2(prf_shake256(coins, nonce)))
        nonce += 1
    e1_rows = []
    for _ in range(K):
        e1_rows.append(sample_poly_cbd2(prf_shake256(coins, nonce)))
        nonce += 1
    e2 = sample_poly_cbd2(prf_shake256(coins, nonce))
    return np.stack(y_rows), np.stack(e1_rows), e2


def byte_decode12(buf: np.ndarray) -> np.ndarray:
    """uint8[384] → int32[256]。"""
    assert buf.shape == (POLY_BYTES,)
    out = np.zeros(N, dtype=np.int32)
    for i in range(N // 2):
        b0 = int(buf[3 * i + 0])
        b1 = int(buf[3 * i + 1])
        b2 = int(buf[3 * i + 2])
        out[2 * i] = b0 | ((b1 & 0x0F) << 8)
        out[2 * i + 1] = (b1 >> 4) | (b2 << 4)
    return out


def compress5(u: int) -> int:
    x = u if u < Q else Q - 1
    d0 = (x * 1290176) & 0xFFFFFFFF
    return ((d0 + (1 << 26)) >> 27) & 0x1F


def compress11(u: int) -> int:
    x = u if u < Q else Q - 1
    d0 = x * 5284526080
    d0 = (d0 + (1 << 32)) >> 33
    return d0 & 0x7FF


def byte_encode5(comp: list[int]) -> bytes:
    out = bytearray(N * 5 // 8)
    for i in range(N // 8):
        t = [comp[8 * i + j] & 0x1F for j in range(8)]
        base = i * 5
        out[base + 0] = 0xFF & ((t[0] >> 0) | (t[1] << 5))
        out[base + 1] = 0xFF & ((t[1] >> 3) | (t[2] << 2) | (t[3] << 7))
        out[base + 2] = 0xFF & ((t[3] >> 1) | (t[4] << 4))
        out[base + 3] = 0xFF & ((t[4] >> 4) | (t[5] << 1) | (t[6] << 6))
        out[base + 4] = 0xFF & ((t[6] >> 2) | (t[7] << 3))
    return bytes(out)


def byte_encode11(comp: list[int]) -> bytes:
    out = bytearray(N * 11 // 8)
    for j in range(N // 8):
        t = [comp[8 * j + k] & 0x7FF for k in range(8)]
        base = 11 * j
        out[base + 0] = (t[0] >> 0) & 0xFF
        out[base + 1] = (t[0] >> 8) | ((t[1] << 3) & 0xFF)
        out[base + 2] = (t[1] >> 5) | ((t[2] << 6) & 0xFF)
        out[base + 3] = (t[2] >> 2) & 0xFF
        out[base + 4] = (t[2] >> 10) | ((t[3] << 1) & 0xFF)
        out[base + 5] = (t[3] >> 7) | ((t[4] << 4) & 0xFF)
        out[base + 6] = (t[4] >> 4) | ((t[5] << 7) & 0xFF)
        out[base + 7] = (t[5] >> 1) & 0xFF
        out[base + 8] = (t[5] >> 9) | ((t[6] << 2) & 0xFF)
        out[base + 9] = (t[6] >> 6) | ((t[7] << 5) & 0xFF)
        out[base + 10] = (t[7] >> 3) & 0xFF
    return bytes(out)


def pack_c(u: np.ndarray, v: np.ndarray) -> bytes:
    parts = []
    for p in range(K):
        parts.append(byte_encode11([compress11(int(x)) for x in u[p].tolist()]))
    parts.append(byte_encode5([compress5(int(x)) for x in v.tolist()]))
    c = b"".join(parts)
    assert len(c) == C_LEN
    return c


def sample_one_poly(rho: bytes, p: int, j: int) -> np.ndarray:
    seed = rho + bytes([j & 0xFF, p & 0xFF])
    xof = shake128_squeeze(seed, XOF_BYTES)
    d1, d2 = unpack_d12_from_xof(xof)
    a_spec = rej_scalar_from_d12(d1, d2)
    a_bulk = rej_bulk_from_d12(d1, d2)
    if not np.array_equal(a_spec, a_bulk):
        raise SystemExit(f"spec vs bulk mismatch at p={p} j={j}")
    return a_spec


def main() -> int:
    inp = _CASE / "input"
    out = _CASE / "output"
    inp.mkdir(parents=True, exist_ok=True)
    out.mkdir(parents=True, exist_ok=True)
    # 清旧 BLOCKED，避免上次缺库残留误导
    for stale in (out / "BLOCKED",):
        if stale.is_file():
            stale.unlink()

    ref = liboqs_ref()
    if ref is None:
        return _blocked(
            out,
            "缺 liboqs / scripts/liboqs_kem_ref；T24 Encaps→Decaps 往返不可用。"
            "请 bash scripts/clone-thirdparty.sh && bash scripts/build-liboqs.sh"
            " && bash scripts/build_liboqs_kem_ref.sh",
        )

    m = FIXED_M
    ek_path = out / "_liboqs_ek.bin"
    dk_path = out / "sk.bin"  # verify：liboqs Decaps(sk, device_c)
    c_liboqs = out / "golden_c.bin"
    k_liboqs = out / "golden_K.bin"

    # —— 权威：liboqs KeyGen（sk 供 Decaps）+ Encaps 诊断 golden（同 m/ek）——
    src_kg = kem_keygen(FIXED_KEM_SEED, ek_path, dk_path)
    if src_kg != "liboqs":
        return _blocked(out, f"kem_keygen 回落为 {src_kg!r}；T24 禁止非 liboqs 权威")
    ek = ek_path.read_bytes()
    sk = dk_path.read_bytes()
    if len(ek) != C_LEN:
        return _blocked(out, f"liboqs ek 长度异常 {len(ek)}")
    if len(sk) != 3168:
        return _blocked(out, f"liboqs sk 长度异常 {len(sk)}（期望 3168）")
    src_enc = kem_encaps(ek, m, c_liboqs, k_liboqs, ek_path=ek_path)
    if src_enc != "liboqs":
        return _blocked(out, f"kem_encaps 回落为 {src_enc!r}；T24 禁止非 liboqs 权威")
    (out / "cross_backend.txt").write_text("liboqs\n", encoding="utf-8")

    rho = extract_rho(ek)
    assert len(rho) == 32

    # —— Host 诊断链（非权威；与设备中间量对拍）——
    K_bytes, coins, h = encaps_head(m, ek)
    liboqs_c = c_liboqs.read_bytes()
    liboqs_k = k_liboqs.read_bytes()
    if len(liboqs_c) != C_LEN or len(liboqs_k) != K_LEN:
        return _blocked(out, f"liboqs encaps 尺寸异常 c={len(liboqs_c)} K={len(liboqs_k)}")
    if liboqs_k != K_bytes:
        raise SystemExit("[gen_data] liboqs K != G(m||H(ek))[:32] — 假绿三问：权威与 SHA3 头不一致")
    mu = mu_embed_from_m(m)

    packed = np.frombuffer(ek[: K * POLY_BYTES], dtype=np.uint8).copy()
    t_hat = np.stack(
        [byte_decode12(packed[p * POLY_BYTES : (p + 1) * POLY_BYTES]) for p in range(K)],
        axis=0,
    )

    y, e1, e2 = sample_y_e1_e2(coins)
    y_e1_e2 = np.concatenate(
        [y.reshape(-1), e1.reshape(-1), e2.reshape(-1)]
    ).astype(np.int32)

    a_hat_flat = np.empty(AHAT_POLYS * N, dtype=np.int32)
    for p in range(K):
        for j in range(K):
            poly = sample_one_poly(rho, p, j)
            off = (p * K + j) * N
            a_hat_flat[off : off + N] = poly
    a_hat = a_hat_flat.reshape(K, K, N)
    y_hat = np.stack(
        [np.asarray(mlkem_ntt(y[i].tolist()), dtype=np.int32) for i in range(K)], axis=0
    )

    zetas_list, gammas_list = load_zetas_gammas()
    zetas = np.asarray(zetas_list, dtype=np.int32)
    gammas = np.asarray(gammas_list, dtype=np.int32)

    rng_mat = np.random.default_rng(SEED_MAT)
    mat_a = rng_mat.integers(-8, 9, size=(16, 32), dtype=np.int8)
    mat_b = rng_mat.integers(-8, 9, size=(32, 32), dtype=np.int8)

    (inp / "ek.bin").write_bytes(ek)
    (inp / "m.bin").write_bytes(m)
    # 禁：coins / K / h / 最终 μ 写入 input（设备 G + Decompress₁）
    zetas.tofile(inp / "zetas.bin")
    gammas.tofile(inp / "gammas.bin")
    mat_a.tofile(inp / "mat_a.bin")
    mat_b.tofile(inp / "mat_b.bin")

    u, v = compute_u_v(a_hat, y_hat, t_hat, e1, e2, mu)
    host_c = pack_c(u, v)
    # 诊断：Host 自洽 c 应与 liboqs 一致；不一致则 gen 失败（防自洽假绿）
    if host_c != liboqs_c:
        mism = sum(1 for a, b in zip(host_c, liboqs_c) if a != b)
        raise SystemExit(
            f"[gen_data] Host Encrypt c ≠ liboqs c（mism~={mism}）—"
            "假绿三问：golden 同源？权威已跑？先修 Host 诊断链再跑设备"
        )
    (out / "golden_c_host.bin").write_bytes(host_c)

    (out / "golden_rho.bin").write_bytes(rho)
    (out / "golden_h.bin").write_bytes(h)
    (out / "golden_coins.bin").write_bytes(coins)
    t_hat.astype(np.int32).tofile(out / "golden_t_hat.bin")
    y_e1_e2.tofile(out / "golden_y_e1_e2.bin")
    a_hat_flat.tofile(out / "golden_a_hat.bin")
    y_hat.astype(np.int32).tofile(out / "golden_y_hat.bin")
    u.astype(np.int32).tofile(out / "golden_u.bin")
    v.astype(np.int32).tofile(out / "golden_v.bin")
    mu.astype(np.int32).tofile(out / "golden_mu.bin")
    # 合法 ek 副本进 input 旁路存档（verify 可读）
    (inp / "ek.bin").write_bytes(ek)

    print(
        f"[gen_data] T24 Encaps→Decaps RT: via={src_enc}; "
        f"ek/sk=liboqs KeyGen; m fixed; sk→Decaps; golden_c/K=liboqs Encaps(diag); "
        f"c={len(c_liboqs.read_bytes())} K={len(k_liboqs.read_bytes())} sk={len(sk)}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
