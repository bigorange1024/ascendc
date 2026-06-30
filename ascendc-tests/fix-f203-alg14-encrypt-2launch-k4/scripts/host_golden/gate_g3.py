#!/usr/bin/env python3
"""
gate_g3.py — G3 golden：Âᵀ·r̂ → u_hat 与 t̂·r̂ → tr_hat（NTT 域，INTT/噪声前）。

自包含：纯 Python Alg.11 basemul + mod q；t̂ 自 ek ByteDecode₁₂；禁止 liboqs。
"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

K = 4
N = 256
Q = 3329
EK_T_BYTES = 1536
POLY_D12_BYTES = 384

GAMMAS = np.array(
    [
        17, 3312, 2761, 568, 583, 2746, 2649, 680, 1637, 1692, 723, 2606, 2288, 1041, 1100, 2229, 1409, 1920,
        2662, 667, 3281, 48, 233, 3096, 756, 2573, 2156, 1173, 3015, 314, 3050, 279, 1703, 1626, 1651, 1678,
        2789, 540, 1789, 1540, 1847, 1482, 952, 2377, 1461, 1868, 2687, 642, 939, 2390, 2308, 1021, 2437, 892,
        2388, 941, 733, 2596, 2337, 992, 268, 3061, 641, 2688, 1584, 1745, 2298, 1031, 2037, 1292, 3220, 109,
        375, 2954, 2549, 780, 2090, 1239, 1645, 1684, 1063, 2266, 319, 3010, 2773, 556, 757, 2572, 2099, 1230,
        561, 2768, 2466, 863, 2594, 735, 2804, 525, 1092, 2237, 403, 2926, 1026, 2303, 1143, 2186, 2150, 1179,
        2775, 554, 886, 2443, 1722, 1607, 1212, 2117, 1874, 1455, 1029, 2300, 2110, 1219, 2935, 394, 885, 2444,
        2154, 1175,
    ],
    dtype=np.int32,
)


def barrett_red(x: int) -> int:
    q = Q
    t = x + (q & (x >> 31))
    t1 = (t * 78) >> 18
    x = t - t1 * q
    t2 = (x * 5039) >> 24
    x = x - t2 * q
    x = x - (q & ~((x - q) >> 31))
    return int(x)


def mod_q_i64(x: int) -> int:
    rem = x % Q
    if rem < 0:
        rem += Q
    return int(rem)


def multiply_ntts(f: np.ndarray, g: np.ndarray) -> np.ndarray:
    h = np.zeros(N, dtype=np.int32)
    for i in range(N // 2):
        gamma = int(GAMMAS[i])
        a0, a1 = int(f[2 * i]), int(f[2 * i + 1])
        b0, b1 = int(g[2 * i]), int(g[2 * i + 1])
        a1b1 = barrett_red(a1 * b1)
        h[2 * i] = barrett_red(a0 * b0 + a1b1 * gamma)
        h[2 * i + 1] = barrett_red(a0 * b1 + a1 * b0)
    return h


def a_hat_offset_at(p: int, j: int) -> int:
    return (j * K + p) * N


def poly_byte_decode12(buf: bytes) -> np.ndarray:
    out = np.empty(N, dtype=np.int32)
    pairs = N // 2
    for i in range(pairs):
        b0, b1, b2 = buf[3 * i], buf[3 * i + 1], buf[3 * i + 2]
        t0 = b0 | ((b1 & 0x0F) << 8)
        t1 = (b1 >> 4) | (b2 << 4)
        out[2 * i] = t0
        out[2 * i + 1] = t1
    return out


def decode_t_hat_from_ek(ek: bytes) -> np.ndarray:
    t_flat = np.empty(K * N, dtype=np.int32)
    for j in range(K):
        off = j * POLY_D12_BYTES
        t_flat[j * N : (j + 1) * N] = poly_byte_decode12(ek[off : off + POLY_D12_BYTES])
    return t_flat


def golden_u_hat(a_hat: np.ndarray, r_hat: np.ndarray) -> np.ndarray:
    u = np.zeros((K, N), dtype=np.int32)
    for p in range(K):
        acc = np.zeros(N, dtype=np.int64)
        for j in range(K):
            a_poly = a_hat[a_hat_offset_at(p, j) : a_hat_offset_at(p, j) + N]
            r_poly = r_hat[j * N : (j + 1) * N]
            prod = multiply_ntts(a_poly, r_poly)
            acc += prod.astype(np.int64)
        u[p] = np.array([mod_q_i64(int(v)) for v in acc], dtype=np.int32)
    return u.reshape(-1)


def golden_tr_hat(t_hat: np.ndarray, r_hat: np.ndarray) -> np.ndarray:
    acc = np.zeros(N, dtype=np.int64)
    for j in range(K):
        t_poly = t_hat[j * N : (j + 1) * N]
        r_poly = r_hat[j * N : (j + 1) * N]
        prod = multiply_ntts(t_poly, r_poly)
        acc += prod.astype(np.int64)
    return np.array([mod_q_i64(int(v)) for v in acc], dtype=np.int32)


def main() -> None:
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} <case_dir> <out_dir>", file=sys.stderr)
        sys.exit(1)
    case_dir = Path(sys.argv[1])
    out_dir = Path(sys.argv[2])

    a_hat_path = out_dir / "a_hat.bin"
    r_hat_path = out_dir / "r_hat.bin"
    ek_path = case_dir / "input" / "ek_pke.bin"
    for p in (a_hat_path, r_hat_path, ek_path):
        if not p.is_file():
            raise SystemExit(f"[gate_g3] missing {p} (run G1/G2 first)")

    a_hat = np.fromfile(a_hat_path, dtype=np.int32)
    r_hat = np.fromfile(r_hat_path, dtype=np.int32)
    ek = ek_path.read_bytes()
    if len(ek) < EK_T_BYTES:
        raise SystemExit(f"[gate_g3] ek too short {len(ek)}")

    t_hat = decode_t_hat_from_ek(ek[:EK_T_BYTES])
    u_hat = golden_u_hat(a_hat, r_hat)
    tr_hat = golden_tr_hat(t_hat, r_hat)

    u_hat.tofile(out_dir / "golden_u_hat.bin")
    tr_hat.tofile(out_dir / "golden_tr_hat.bin")
    t_hat.tofile(out_dir / "golden_t_hat.bin")
    print(f"[gate_g3] u_hat/tr_hat golden OK u_hat={u_hat.nbytes}B tr_hat={tr_hat.nbytes}B")


if __name__ == "__main__":
    main()
