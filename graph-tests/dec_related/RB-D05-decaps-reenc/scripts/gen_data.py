#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RB-D05 gen_data：Decaps Re-Encrypt 输入 + golden。

契约：
  - 输入：ek[1568]、m'[32]、r'[32]（coins）；禁写最终 c'/μ/t̂/y/e/Â/ŷ/u/v 到 input
  - 权威 c'：优先 scripts/liboqs_pke_ref encrypt；缺库 → host FIPS Encrypt（cross_backend.txt）
  - 诊断 golden：host 拓扑（Decode₁₂/CBD/Â/ŷ/u/v/μ）供中间量对拍

本刀可独立喂 bin（不依赖 K01 设备输出）；coins 语义对齐 K01 的 r'。
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

sys.path.insert(0, str(_SCRIPT))
sys.path.insert(0, str(_REPO / "graph-tests" / "enc_related" / "RB-T07-prep-shell" / "scripts"))
sys.path.insert(0, str(_REPO / "library" / "shared" / "fips203_se_sample"))
from prep_host import extract_rho  # noqa: E402
from golden_se_sampling import sample_poly_cbd2  # noqa: E402
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

SEED = int(os.environ.get("SEED_D", "20260909"))
SEED_MAT = 42
C_LEN = 1568
EK_BYTES = 1568
DK_BYTES = 1536
M_BYTES = 32
COINS_BYTES = 32
AHAT_POLYS = K * K
PRF_OUT = 2 * N // 4  # 128
POLY_BYTES = 384

REF_BIN = _REPO / "scripts" / "liboqs_pke_ref"
BUILD_REF = _REPO / "scripts" / "build_liboqs_pke_ref_mlkem1024.sh"


def _try_liboqs_ref():
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


def prf_shake256(coins: bytes, nonce: int) -> bytes:
    import hashlib

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


def byte_encode12(coeffs: np.ndarray) -> np.ndarray:
    assert coeffs.shape == (N,)
    out = np.zeros(POLY_BYTES, dtype=np.uint8)
    for i in range(N // 2):
        v0 = int(coeffs[2 * i]) & 0xFFF
        v1 = int(coeffs[2 * i + 1]) & 0xFFF
        out[3 * i + 0] = v0 & 0xFF
        out[3 * i + 1] = ((v0 >> 8) & 0x0F) | ((v1 & 0x0F) << 4)
        out[3 * i + 2] = (v1 >> 4) & 0xFF
    return out


def byte_decode12(buf: np.ndarray) -> np.ndarray:
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


def decode_t_hat_from_ek(ek: bytes) -> np.ndarray:
    body = np.frombuffer(ek[: K * POLY_BYTES], dtype=np.uint8).copy()
    rows = []
    for p in range(K):
        enc = body[p * POLY_BYTES : (p + 1) * POLY_BYTES]
        rows.append(byte_decode12(enc))
    return np.stack(rows, axis=0)


def host_encrypt_intermediates(ek: bytes, m_prime: bytes, coins: bytes):
    """Host FIPS 拓扑中间量（诊断）；c 亦作缺库时权威。"""
    rho = extract_rho(ek)
    t_hat = decode_t_hat_from_ek(ek)
    mu = mu_embed_from_m(m_prime)
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
    u, v = compute_u_v(a_hat, y_hat, t_hat, e1, e2, mu)
    golden_c = pack_c(u, v)
    return {
        "rho": rho,
        "t_hat": t_hat,
        "mu": mu,
        "y_e1_e2": y_e1_e2,
        "a_hat_flat": a_hat_flat,
        "y_hat": y_hat,
        "u": u,
        "v": v,
        "golden_c": golden_c,
    }


def write_common_inputs(inp: Path, out: Path, ek: bytes, m_prime: bytes, coins: bytes, inter: dict):
    (inp / "ek.bin").write_bytes(ek)
    (inp / "m_prime.bin").write_bytes(m_prime)
    (inp / "r_prime.bin").write_bytes(coins)
    zetas_list, gammas_list = load_zetas_gammas()
    np.asarray(zetas_list, dtype=np.int32).tofile(inp / "zetas.bin")
    np.asarray(gammas_list, dtype=np.int32).tofile(inp / "gammas.bin")
    rng_mat = np.random.default_rng(SEED_MAT)
    rng_mat.integers(-8, 9, size=(16, 32), dtype=np.int8).tofile(inp / "mat_a.bin")
    rng_mat.integers(-8, 9, size=(32, 32), dtype=np.int8).tofile(inp / "mat_b.bin")

    (out / "golden_rho.bin").write_bytes(inter["rho"])
    inter["t_hat"].astype(np.int32).tofile(out / "golden_t_hat.bin")
    inter["y_e1_e2"].tofile(out / "golden_y_e1_e2.bin")
    inter["a_hat_flat"].tofile(out / "golden_a_hat.bin")
    inter["y_hat"].astype(np.int32).tofile(out / "golden_y_hat.bin")
    inter["u"].astype(np.int32).tofile(out / "golden_u.bin")
    inter["v"].astype(np.int32).tofile(out / "golden_v.bin")
    inter["mu"].astype(np.int32).tofile(out / "golden_mu.bin")
    (out / "golden_c_prime.bin").write_bytes(inter["golden_c"])


def _liboqs_fixture(inp: Path, out: Path, ref: Path) -> str:
    """liboqs KeyGen+Encrypt → 权威 golden_c'；中间量仍走 host 拓扑。"""
    rng = np.random.default_rng(SEED)
    d = bytes(rng.integers(0, 256, size=32, dtype=np.uint8).tolist())
    m_prime = bytes(rng.integers(0, 256, size=M_BYTES, dtype=np.uint8).tolist())
    coins = bytes(rng.integers(0, 256, size=COINS_BYTES, dtype=np.uint8).tolist())

    with tempfile.TemporaryDirectory(prefix="rb_d05_") as td:
        tdp = Path(td)
        ek = tdp / "ek.bin"
        dk = tdp / "dk.bin"
        c = tdp / "c.bin"
        m_path = tdp / "m.bin"
        coins_path = tdp / "coins.bin"
        m_path.write_bytes(m_prime)
        coins_path.write_bytes(coins)

        subprocess.check_call([str(ref), "keygen", str(ek), str(dk), d.hex()])
        if ek.stat().st_size != EK_BYTES or dk.stat().st_size != DK_BYTES:
            raise SystemExit(
                f"[BLOCKED] keygen size ek={ek.stat().st_size} dk={dk.stat().st_size}"
            )
        subprocess.check_call(
            [str(ref), "encrypt", str(c), str(ek), str(m_path), str(coins_path)]
        )
        if c.stat().st_size != C_LEN:
            raise SystemExit(f"[BLOCKED] encrypt c size={c.stat().st_size}")

        ek_bytes = ek.read_bytes()
        c_bytes = c.read_bytes()

    inter = host_encrypt_intermediates(ek_bytes, m_prime, coins)
    # 设备路径与 host 拓扑同式；须与 liboqs 逐字节一致，否则假绿/假红风险
    if inter["golden_c"] != c_bytes:
        mism = sum(1 for a, b in zip(inter["golden_c"], c_bytes) if a != b) + abs(
            len(inter["golden_c"]) - len(c_bytes)
        )
        raise SystemExit(
            f"[BLOCKED] host FIPS pack_c ≠ liboqs encrypt (mism~={mism}); "
            "勿用分叉 oracle；先修 host 拓扑或换向量"
        )
    write_common_inputs(inp, out, ek_bytes, m_prime, coins, inter)
    (out / "cross_backend.txt").write_text("liboqs_pke_ref\n", encoding="utf-8")
    return "liboqs_pke_ref"


def _host_fips_fixture(inp: Path, out: Path) -> str:
    """缺 liboqs：合成 ek + host Encrypt 作权威（FEEDBACK 须标明）。"""
    rng = np.random.default_rng(SEED)

    def rand_hat_poly() -> np.ndarray:
        time = rng.integers(0, Q, size=N, dtype=np.int32)
        return np.asarray(mlkem_ntt(time.tolist()), dtype=np.int32) % Q

    t_hat = np.stack([rand_hat_poly() for _ in range(K)], axis=0)
    packed = np.zeros(K * POLY_BYTES, dtype=np.uint8)
    for p in range(K):
        packed[p * POLY_BYTES : (p + 1) * POLY_BYTES] = byte_encode12(t_hat[p])
    # ρ：固定可复现 32B
    rho = bytes([(0x11 + i) & 0xFF for i in range(32)])
    ek = packed.tobytes() + rho
    assert len(ek) == EK_BYTES
    m_prime = bytes(rng.integers(0, 256, size=M_BYTES, dtype=np.uint8).tolist())
    coins = bytes(rng.integers(0, 256, size=COINS_BYTES, dtype=np.uint8).tolist())
    inter = host_encrypt_intermediates(ek, m_prime, coins)
    write_common_inputs(inp, out, ek, m_prime, coins, inter)
    (out / "cross_backend.txt").write_text("host_fips\n", encoding="utf-8")
    print(
        "[gen_data] WARN: liboqs_pke_ref 不可用 → 回落 host FIPS Encrypt oracle；"
        "FEEDBACK 须标明非 liboqs"
    )
    return "host_fips"


def main() -> int:
    inp = _CASE / "input"
    out = _CASE / "output"
    inp.mkdir(parents=True, exist_ok=True)
    out.mkdir(parents=True, exist_ok=True)

    ref = _try_liboqs_ref()
    if ref is not None:
        backend = _liboqs_fixture(inp, out, ref)
    else:
        backend = _host_fips_fixture(inp, out)

    print(
        f"[gen_data] RB-D05 Re-Encrypt: ek+m'+r'→c' via {backend}; "
        f"c'={C_LEN}B SEED_D={SEED}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
