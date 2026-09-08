#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
RB-T24 verify：PASS_SYNC + PASS_RT（主：liboqs Decaps(sk,c)→K' ≡ K_dev）+ PASS_IO。

硬门禁：
  - 缺 liboqs / cross_backend≠liboqs → FAIL/BLOCKED（勿假绿；禁止 python Decaps 冒充权威）
  - 设备 Encaps 出 (c,K)；权威 Decaps(sk,c)→K' 与 K 逐字节一致
  - out 魔数 T24=0x543F0018；TRACE 含 G_DONE
"""
from __future__ import annotations

import hashlib
import struct
import sys
from pathlib import Path

import numpy as np

_SCRIPT = Path(__file__).resolve().parent
ROOT = _SCRIPT.parent
OUT = ROOT / "output"
INP = ROOT / "input"
_REPO = ROOT.parents[2]

sys.path.insert(0, str(_REPO / "library" / "shared" / "f203_kem_ref"))
from kem_ref import kem_decaps, liboqs_ref  # noqa: E402

MAGIC_OUT_OK = 0x543F0018
K_LEN = 32
C_LEN = 1568
DK_LEN = 3168

HARD = {
    0: 0x484F5354,  # HOST_PRE
    1: 0x50524550,  # PREP_DONE
    2: 0xA1010001,  # AIV0_PRE_SET1_NTT
    6: 0xA1030003,  # AIV0_POST_WAIT3_NTT
    8: 0xA1040004,  # AIV0_PRE_SET4
    10: 0x484F4D49,  # HOST_MID_SYNC
    11: 0xA1011001,  # AIV0_PRE_SET1_INTT
    14: 0xA1031003,  # AIV0_POST_WAIT3_INTT
    16: 0x5041434B,  # PACK_DONE
    17: 0x484F5355,  # HOST_POST
    22: 0x41484154,  # AHAT_DONE
    23: 0x59484154,  # YHAT_DONE
    24: 0x43424439,  # CBD_DONE
    25: 0x42443132,  # BD12_DONE
    26: 0x4D553031,  # MU_DONE
    27: 0x47303132,  # G_DONE
}

SOFT = {
    3: 0xA1110001,  # AIV1_PRE_SET1_NTT
    4: 0xC1010001,  # AIC_POST_WAIT1_NTT
    5: 0xC1030003,  # AIC_PRE_SET3_NTT
    7: 0x4D554C44,  # MUL_DONE
    9: 0xC1040004,  # AIC_POST_WAIT4
    12: 0xC1011001,  # AIC_POST_WAIT1_INTT
    13: 0xC1031003,  # AIC_PRE_SET3_INTT
    15: 0x5556444E,  # UV_DONE
    18: 0xA1130003,  # AIV1_POST_WAIT3_NTT
    19: 0xA1140004,  # AIV1_PRE_SET4
    20: 0xA1111001,  # AIV1_PRE_SET1_INTT
    21: 0xA1131003,  # AIV1_POST_WAIT3_INTT
}


def _non_zero(path: Path, label: str) -> bool:
    if not path.is_file():
        print(f"[FAIL] missing {label}", file=sys.stderr)
        return False
    data = path.read_bytes()
    if len(data) < 4 or all(b == 0 for b in data):
        print(f"[FAIL] {label} all-zero — Cube 未写出", file=sys.stderr)
        return False
    return True


def _cmp_bytes(got: Path, golden: Path, label: str) -> bool:
    if not got.is_file() or not golden.is_file():
        print(f"[PASS_IO=0] missing {label} or golden")
        return False
    a, b = got.read_bytes(), golden.read_bytes()
    if a != b:
        mism = sum(1 for x, y in zip(a, b) if x != y) + abs(len(a) - len(b))
        print(f"[PASS_IO=0] {label} mismatch bytes~={mism} len={len(a)}/{len(b)}")
        return False
    print(f"[PASS_IO] {label} match")
    return True


def _cmp_i32(got: Path, golden: Path, label: str) -> bool:
    if not got.is_file() or not golden.is_file():
        print(f"[PASS_IO=0] missing {label} or golden")
        return False
    a = np.fromfile(got, dtype=np.int32)
    b = np.fromfile(golden, dtype=np.int32)
    if a.shape != b.shape:
        print(f"[PASS_IO=0] {label} shape {a.shape} != {b.shape}")
        return False
    diff = int(np.max(np.abs(a.astype(np.int64) - b.astype(np.int64)))) if a.size else 0
    if diff != 0:
        mism = int(np.count_nonzero(a != b))
        print(f"[PASS_IO=0] {label} max_abs={diff} mism={mism}/{a.size}")
        return False
    print(f"[PASS_IO] {label} max_abs=0 n={a.size}")
    return True


def _recompute_head() -> tuple[bytes, bytes, bytes] | None:
    """自洽：h=H(ek)，K‖r=G(m‖h)。"""
    m_p, ek_p = INP / "m.bin", INP / "ek.bin"
    if not m_p.is_file() or not ek_p.is_file():
        return None
    m, ek = m_p.read_bytes(), ek_p.read_bytes()
    if len(m) != 32 or len(ek) != C_LEN:
        return None
    h = hashlib.sha3_256(ek).digest()
    g = hashlib.sha3_512(m + h).digest()
    return h, g[:32], g[32:]


def main() -> int:
    if (OUT / "BLOCKED").is_file():
        print(f"[FAIL] BLOCKED: {(OUT / 'BLOCKED').read_text(encoding='utf-8').strip()}", file=sys.stderr)
        return 2

    backend_p = OUT / "cross_backend.txt"
    if not backend_p.is_file() or backend_p.read_text(encoding="utf-8").strip() != "liboqs":
        print(
            "[FAIL] cross_backend 非 liboqs — T24 禁止无权威往返的假绿",
            file=sys.stderr,
        )
        return 2

    if liboqs_ref() is None:
        print("[FAIL] 缺 liboqs_kem_ref — Decaps 权威不可用", file=sys.stderr)
        return 2

    out_p = OUT / "out.bin"
    tr_p = OUT / "trace.bin"
    if not out_p.is_file() or not tr_p.is_file():
        print("[FAIL] missing output/out.bin or output/trace.bin", file=sys.stderr)
        return 1
    out = out_p.read_bytes()
    if len(out) < 4:
        print("[FAIL] out.bin too short", file=sys.stderr)
        return 1
    (out_magic,) = struct.unpack_from("<I", out, 0)
    if out_magic != MAGIC_OUT_OK:
        print(f"[FAIL] out magic 0x{out_magic:08X} != 0x{MAGIC_OUT_OK:08X}", file=sys.stderr)
        return 1
    tr = tr_p.read_bytes()
    if len(tr) < 28 * 4:
        print("[FAIL] trace.bin too short", file=sys.stderr)
        return 1
    vals = list(struct.unpack_from("<28I", tr, 0))
    for slot, want in HARD.items():
        got = vals[slot]
        if got != want:
            print(f"[FAIL] TRACE[{slot}]=0x{got:08X} expect 0x{want:08X}", file=sys.stderr)
            return 1
    for slot, want in SOFT.items():
        got = vals[slot]
        if got != want:
            print(f"[WARN] TRACE[{slot}]=0x{got:08X} (soft; SIM 上 AIC/AIV1 标量 TRACE 常空)")
    if not _non_zero(OUT / "mat_c_ntt.bin", "mat_c_ntt.bin"):
        return 1
    if not _non_zero(OUT / "mat_c_intt.bin", "mat_c_intt.bin"):
        return 1

    print("[PASS_SYNC] dual-launch causal + G + MU + BD12 + CBD + AHAT/YHAT + dual Cube + pack TRACE ok")

    # —— 主验收：设备 (c,K) → liboqs Decaps(sk,c)→K' ≡ K ——
    c_p = OUT / "c.bin"
    k_p = OUT / "K_dev.bin"
    sk_p = OUT / "sk.bin"
    if not c_p.is_file() or not k_p.is_file() or not sk_p.is_file():
        print("[FAIL] missing c.bin / K_dev.bin / sk.bin", file=sys.stderr)
        return 1
    c = c_p.read_bytes()
    k_dev = k_p.read_bytes()
    sk = sk_p.read_bytes()
    if len(c) != C_LEN or len(k_dev) != K_LEN or len(sk) != DK_LEN:
        print(
            f"[FAIL] 尺寸异常 c={len(c)} K={len(k_dev)} sk={len(sk)}",
            file=sys.stderr,
        )
        return 1

    k_prime_p = OUT / "K_prime_liboqs_decaps.bin"
    src = kem_decaps(sk, c, k_prime_p, dk_path=sk_p, c_path=c_p)
    if src != "liboqs":
        print(f"[FAIL] kem_decaps 回落为 {src!r}；T24 禁止非 liboqs 权威", file=sys.stderr)
        return 2
    k_prime = k_prime_p.read_bytes()
    if len(k_prime) != K_LEN:
        print(f"[FAIL] Decaps K' 长度异常 {len(k_prime)}", file=sys.stderr)
        return 1
    if k_prime != k_dev:
        mism = sum(1 for a, b in zip(k_prime, k_dev) if a != b)
        print(
            f"[FAIL] PASS_RT=0 — Decaps(sk,c)→K' ≠ K_dev（mism~={mism}）",
            file=sys.stderr,
        )
        return 1
    print("[PASS_RT] liboqs Decaps(sk, device_c)→K' ≡ K_dev")

    # 辅：K 应 ≡ G(m‖H(ek))[:32]；可选与 liboqs Encaps golden 交叉
    recomputed = _recompute_head()
    if recomputed is None:
        print("[PASS_RT=0] cannot recompute G/H from input m/ek", file=sys.stderr)
        return 1
    h_ref, k_ref, coins_ref = recomputed
    if k_dev != k_ref:
        print("[FAIL] K_dev != G(m||H(ek))[:32]", file=sys.stderr)
        return 1
    print("[PASS_RT] K_dev ≡ G(m||H(ek))[:32]")
    if (OUT / "h_dev.bin").is_file() and (OUT / "h_dev.bin").read_bytes() != h_ref:
        print("[FAIL] h_dev != H(ek)", file=sys.stderr)
        return 1
    if (OUT / "coins_dev.bin").is_file() and (OUT / "coins_dev.bin").read_bytes() != coins_ref:
        print("[FAIL] coins_dev != G[32:]", file=sys.stderr)
        return 1
    if (OUT / "golden_c.bin").is_file():
        if not _cmp_bytes(OUT / "c.bin", OUT / "golden_c.bin", "c↔liboqs_Encaps(diag)"):
            print("[FAIL] 设备 c ≠ liboqs Encaps 诊断 golden（往返前交叉已红）", file=sys.stderr)
            return 1
    if (OUT / "golden_K.bin").is_file():
        if k_dev != (OUT / "golden_K.bin").read_bytes():
            print("[FAIL] K_dev ≠ liboqs Encaps golden_K", file=sys.stderr)
            return 1
        print("[PASS_IO] K_dev match liboqs Encaps golden_K")

    io_ok = True
    io_ok = _cmp_bytes(OUT / "coins_dev.bin", OUT / "golden_coins.bin", "coins_dev") and io_ok
    io_ok = _cmp_bytes(OUT / "h_dev.bin", OUT / "golden_h.bin", "h_dev") and io_ok
    io_ok = _cmp_i32(OUT / "mu_dev.bin", OUT / "golden_mu.bin", "mu_dev") and io_ok
    io_ok = _cmp_i32(OUT / "u.bin", OUT / "golden_u.bin", "u") and io_ok
    io_ok = _cmp_i32(OUT / "v.bin", OUT / "golden_v.bin", "v") and io_ok
    io_ok = _cmp_i32(OUT / "t_hat_dev.bin", OUT / "golden_t_hat.bin", "t_hat") and io_ok
    io_ok = _cmp_i32(OUT / "a_hat.bin", OUT / "golden_a_hat.bin", "a_hat") and io_ok
    io_ok = _cmp_i32(OUT / "y_hat.bin", OUT / "golden_y_hat.bin", "y_hat") and io_ok
    io_ok = _cmp_bytes(OUT / "y_e1_e2_dev.bin", OUT / "golden_y_e1_e2.bin", "y_e1_e2_dev") and io_ok
    io_ok = _cmp_bytes(OUT / "rho_dev.bin", OUT / "golden_rho.bin", "rho_dev") and io_ok

    if io_ok:
        print("[SUCCESS] PASS_SYNC + PASS_RT + PASS_IO — RB-T24 Encaps→liboqs Decaps roundtrip")
        return 0
    print("[FAIL] PASS_RT ok but PASS_IO=0 — 中间量对拍失败", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
