#!/usr/bin/env python3
"""
golden_m.py — FIPS 203 Alg.15 全链 Host golden：dk_PKE + c → m（32B）。

流水线位置：gen_data（DECRYPT_VERIFY=1）写出 output/golden_m.bin；
verify_result 与设备 output/m.bin 对拍。

语义对齐设备 1-kernel fused：
  ByteDecode₁₂(dk) → ŝ；unpack(c)→u',v'；NTT(u')→û；
  ⟨ŝ,û⟩→ŵ；pad+INTT→w；Compress₁(v'−w)+Encode₁→m。

禁止 liboqs；仅 I/O 等价验收，非 AscendC 实现规格。
"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

from f203_ref_common import (
    K,
    N,
    Q,
    mod_q_i64,
    multiply_ntts,
    stage123_transform,
)

DK_BYTES = 768
CT_BYTES = 768
MSG_BYTES = 32
C1_BYTES = 640
C1_POLY_BYTES = 320
C2_BYTES = 128


def poly_byte_decode12(buf: bytes) -> np.ndarray:
    """
    FIPS ByteDecode₁₂：384B → n=256 个 12-bit 系数（int32）。

    每 3 字节装 2 个系数：低 12 bit / 高 12 bit。
    """
    out = np.empty(N, dtype=np.int32)
    for i in range(N // 2):
        b0, b1, b2 = buf[3 * i], buf[3 * i + 1], buf[3 * i + 2]
        out[2 * i] = b0 | ((b1 & 0x0F) << 8)
        out[2 * i + 1] = (b1 >> 4) | (b2 << 4)
    return out


def decode_s_hat(dk: bytes) -> np.ndarray:
    """
    Alg.15 行 5：ŝ ← ByteDecode₁₂(dk_PKE)。

    @param dk 768B = k×384
    @return shape (k,n) int32
    """
    s = np.empty((K, N), dtype=np.int32)
    for j in range(K):
        s[j] = poly_byte_decode12(dk[j * 384 : (j + 1) * 384])
    return s


def byte_decode_d(bits_src: bytes, d: int) -> np.ndarray:
    """
    FIPS ByteDecode_d：按 bit 流解出 n 个 d-bit 整数。

    @param bits_src 打包字节（长度 ≥ 32*d）
    @param d 位宽（本探针用 10 / 4）
    """
    out = np.empty(N, dtype=np.int32)
    bit_pos = 0
    mask = (1 << d) - 1
    for i in range(N):
        a = 0
        for j in range(d):
            byte_idx = bit_pos >> 3
            bit_idx = bit_pos & 7
            if (bits_src[byte_idx] >> bit_idx) & 1:
                a |= 1 << j
            bit_pos += 1
        out[i] = a & mask
    return out


def decompress_d_scalar(u: int, d: int) -> int:
    """
    FIPS Decompress_d：把 d-bit 压缩值映回 Z_q。

    公式 round(u·q / 2^d)，用偏置移位实现（本探针默认 d∈{10,4}（保留 11/5 分支仅作调试兼容））。
    """
    u = int(u)
    if d == 11:
        return (u * Q + 1024) >> 11
    if d == 5:
        return (u * Q + 16) >> 5
    if d == 10:
        return (u * Q + 512) >> 10
    if d == 4:
        return (u * Q + 8) >> 4
    raise ValueError(f"d={d}")


def unpack_ciphertext(c: bytes) -> tuple[np.ndarray, np.ndarray]:
    """
    Alg.15 行 3–4：c=c₁‖c₂ → (u', v')。

    c₁：k×320B，d=10；c₂：128B，d=4。
    @return u shape (k,n)，v shape (n)，均为 int32
    """
    u = np.empty((K, N), dtype=np.int32)
    for p in range(K):
        chunk = c[p * C1_POLY_BYTES : (p + 1) * C1_POLY_BYTES]
        comp = byte_decode_d(chunk, 10)
        u[p] = np.array([decompress_d_scalar(int(x), 10) for x in comp], dtype=np.int32)
    comp_v = byte_decode_d(c[C1_BYTES : C1_BYTES + C2_BYTES], 4)
    v = np.array([decompress_d_scalar(int(x), 4) for x in comp_v], dtype=np.int32)
    return u, v


def compress_1_scalar(x: int) -> int:
    """
    Compress₁：对齐 liboqs mlk_scalar_compress_d1（Barrett），非旧 G4 的 (Q+1)/2。

    与 ((2x + Q//2)//Q)%2 在 u∈[0,q) 全量一致；与旧 G4 ((2x+(Q+1)//2)//Q)&1
    仅在 u=832 差 1 bit。设备向量尾段用同一 Barrett 常数 1290168。
    """
    u = mod_q_i64(int(x))
    d0 = (u * 1290168) & 0xFFFFFFFF
    return ((d0 + (1 << 30)) & 0xFFFFFFFF) >> 31


def extract_message(w: np.ndarray) -> bytes:
    """
    Alg.15 行 6–7 尾：对 w[i]= (v'−w_time)[i] 做 Compress₁，按 bit 拼成 32B。

    位序：系数 i → 字节 i>>3 的 bit (i&7)（小端 bit）。
    """
    msg = bytearray(MSG_BYTES)
    for i in range(N):
        bit = compress_1_scalar(int(w[i]))
        if bit:
            msg[i >> 3] |= 1 << (i & 7)
    return bytes(msg)


def golden_w_hat(s_hat: np.ndarray, u_hat: np.ndarray) -> np.ndarray:
    """
    Alg.15 行 6：ŵ ← Σ_j MultiplyNTTs(ŝ_j, û_j) mod q。

    @param s_hat / u_hat shape (k,n)
    @return shape (n,) int32
    """
    acc = np.zeros(N, dtype=np.int64)
    for j in range(K):
        prod = multiply_ntts(s_hat[j], u_hat[j])
        acc += prod.astype(np.int64)
    return np.array([mod_q_i64(int(v)) for v in acc], dtype=np.int32)


def golden_decrypt(dk: bytes, c: bytes) -> bytes:
    """
    Alg.15 全链 Host 参考（与 1-kernel fused 段序一致）。

    pad：ŵ 放在 polyvec 第 0 槽，其余 0，供 Stage123 INTT（与设备 wPadded 一致）。
    """
    s_hat = decode_s_hat(dk)
    u, v = unpack_ciphertext(c)
    u_hat = stage123_transform(u, "ntt")
    w_hat = golden_w_hat(s_hat, u_hat)
    # 设备：pad→wPadded 再 INTT；此处同构
    w_pad = np.zeros((K, N), dtype=np.int32)
    w_pad[0] = w_hat
    w_time = stage123_transform(w_pad, "intt")[0]
    w = np.array([mod_q_i64(int(v[i]) - int(w_time[i])) for i in range(N)], dtype=np.int32)
    return extract_message(w)


def main() -> None:
    """CLI：dk_pke + c.bin → golden_m.out（32B）。"""
    if len(sys.argv) != 4:
        print(f"usage: {sys.argv[0]} <dk_pke> <c.bin> <golden_m.out>", file=sys.stderr)
        sys.exit(1)
    dk = Path(sys.argv[1]).read_bytes()
    c = Path(sys.argv[2]).read_bytes()
    out_path = Path(sys.argv[3])
    if len(dk) != DK_BYTES or len(c) != CT_BYTES:
        raise SystemExit(f"bad sizes dk={len(dk)} c={len(c)}")
    m = golden_decrypt(dk, c)
    if len(m) != MSG_BYTES:
        raise SystemExit(f"golden m size {len(m)} != {MSG_BYTES}")
    out_path.write_bytes(m)
    print(f"[golden_m] OK {len(m)}B")


if __name__ == "__main__":
    main()
