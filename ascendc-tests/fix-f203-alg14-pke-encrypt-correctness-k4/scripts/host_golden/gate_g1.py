#!/usr/bin/env python3
"""
gate_g1.py — G1 分阶段 golden：a_hat（ρ→SampleNTT×16）与 r/e1/e2（coins→PRF+CBD×9）。

自包含：抄写 FIPS 203 语义，禁止 liboqs；与 vendored 设备路径 I/O 一致。
"""
from __future__ import annotations

import hashlib
import struct
import sys
from pathlib import Path

import numpy as np

K = 4
N = 256
Q = 3329
XOF_BYTES = 672
CAND_PAIRS = XOF_BYTES // 3
PRF_BYTES = 128
PRF_BATCH = 9


def a_hat_offset(p: int, j: int) -> int:
    return (p * K + j) * N


def shake128_squeeze(msg: bytes, outlen: int) -> bytes:
    return hashlib.shake_128(msg).digest(outlen)


def unpack_d12_from_xof(buf: bytes) -> tuple[np.ndarray, np.ndarray]:
    d1 = np.empty(CAND_PAIRS, dtype=np.int32)
    d2 = np.empty(CAND_PAIRS, dtype=np.int32)
    pos = 0
    for t in range(CAND_PAIRS):
        c0, c1, c2 = buf[pos], buf[pos + 1], buf[pos + 2]
        d1[t] = c0 + 256 * (c1 & 0x0F)
        d2[t] = (c1 >> 4) + 16 * c2
        pos += 3
    return d1, d2


def rej_scalar_from_d12(d1: np.ndarray, d2: np.ndarray) -> np.ndarray:
    out: list[int] = []
    for i in range(d1.shape[0]):
        v1 = int(d1[i])
        if v1 < Q and len(out) < N:
            out.append(v1)
        v2 = int(d2[i])
        if v2 < Q and len(out) < N:
            out.append(v2)
    if len(out) < N:
        raise SystemExit(f"rej: only {len(out)} coeffs")
    return np.array(out[:N], dtype=np.int32)


def sample_ntt_one_poly(rho: bytes, p: int, j: int) -> np.ndarray:
    seed = rho + bytes([j & 0xFF, p & 0xFF])
    xof = shake128_squeeze(seed, XOF_BYTES)
    d1, d2 = unpack_d12_from_xof(xof)
    return rej_scalar_from_d12(d1, d2)


def build_a_hat(rho: bytes) -> np.ndarray:
    polys = K * K
    a_hat = np.empty(polys * N, dtype=np.int32)
    for p in range(K):
        for j in range(K):
            poly = sample_ntt_one_poly(rho, p, j)
            off = a_hat_offset(p, j)
            a_hat[off : off + N] = poly
    return a_hat


def prf_shake256(coins: bytes, nonce: int) -> bytes:
    return hashlib.shake_256(coins + bytes([nonce & 0xFF])).digest(PRF_BYTES)


def _load32_le(buf: bytes, off: int) -> int:
    return int(buf[off]) | (int(buf[off + 1]) << 8) | (int(buf[off + 2]) << 16) | (int(buf[off + 3]) << 24)


def sample_poly_cbd2(buf: bytes) -> np.ndarray:
    coeffs = np.zeros(N, dtype=np.int32)
    for i in range(N // 8):
        t = _load32_le(buf, 4 * i)
        d = (t & 0x55555555) + ((t >> 1) & 0x55555555)
        for j in range(8):
            a = (d >> (4 * j + 0)) & 0x3
            b = (d >> (4 * j + 2)) & 0x3
            c = a - b
            if c < 0:
                c += Q
            coeffs[8 * i + j] = c % Q
    return coeffs


def build_re(coins: bytes) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    rows = [sample_poly_cbd2(prf_shake256(coins, nonce)) for nonce in range(PRF_BATCH)]
    stacked = np.stack(rows)
    r = stacked[0:K]
    e1 = stacked[K : 2 * K]
    e2 = stacked[2 * K]
    return r, e1, e2


def main() -> None:
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} <case_dir> <out_dir>", file=sys.stderr)
        sys.exit(1)
    case_dir = Path(sys.argv[1])
    out_dir = Path(sys.argv[2])
    out_dir.mkdir(parents=True, exist_ok=True)

    ek = (case_dir / "input" / "ek_pke.bin").read_bytes()
    if len(ek) != 1568:
        raise SystemExit(f"bad ek_pke.bin len={len(ek)}")
    rho = ek[1536:1568]

    coins = (case_dir / "input" / "coins.bin").read_bytes()
    if len(coins) != 32:
        raise SystemExit(f"bad coins.bin len={len(coins)}")

    a_hat = build_a_hat(rho)
    r, e1, e2 = build_re(coins)

    a_hat.tofile(out_dir / "golden_a_hat.bin")
    r.astype(np.int32).tofile(out_dir / "golden_r.bin")
    e1.astype(np.int32).tofile(out_dir / "golden_e1.bin")
    e2.astype(np.int32).tofile(out_dir / "golden_e2.bin")

    meta = struct.pack("<II", int(a_hat.nbytes), int(r.nbytes + e1.nbytes + e2.nbytes))
    (out_dir / "golden_g1_meta.bin").write_bytes(meta)
    print(f"[gate_g1] rho tail OK a_hat={a_hat.nbytes}B r={r.nbytes}B e1={e1.nbytes}B e2={e2.nbytes}B")


if __name__ == "__main__":
    main()
