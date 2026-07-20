#!/usr/bin/env python3
"""
liboqs_kem_fixture.py — 生成 liboqs KEM 全链黑盒向量（ml_kem_1024 / k=4）。

覆盖 KeyGen / Encaps / Decaps 三阶段 + 隐式拒绝路径，供 `liboqs_kem_vs_ascendc.sh`
与 `stable_kem_liboqs_roundtrip.sh` 逐级与 AscendC 输出对拍。

两种种子模式（互斥）：

1) **--random**（推荐办公室 round-trip）：
   - kem_seed = os.urandom(64) = d(32)‖z(32)  —— 与 liboqs keypair_derand 输入同字节
   - m        = os.urandom(32)                 —— 与 liboqs encaps_derand 输入同字节
   - AscendC 侧须：KeyGen `KEM_KG_EXT_SEED=1` 吃 kem_seed.bin；Encaps `M_FILE=m.bin`

2) **--seed-d / SEED_D**（定点 derand，旧路径）：
   - d = SHA3-256("exp-mlkem-f203-2s1e-k4:SEED_D={seed}")
   - z = SHA3-256("exp-mlkem-f203-kem-k4:SEED_Z={seed}")
   - m = SHA3-256("exp-mlkem-f203-kem-encaps-k4:SEED_M={seed}")
   - 供生产路径（device 内自派生 d/z）与固定 SEED_D 回归

产出（写入 --out-dir）：
- kem_seed.bin / m.bin / d.bin / z.bin         ：喂 AscendC / 调试
- seed_d.bin                                   ：仅定点模式有意义（uint32 LE）
- ek_kem.bin / dk_kem.bin / c.bin / K.bin …
- K_decaps.bin / c_bad.bin / K_reject.bin
"""
from __future__ import annotations

import argparse
import hashlib
import os
import struct
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
FIPS203_SE = REPO_ROOT / "library/shared/fips203_se_sample"
REF_BIN = REPO_ROOT / "scripts/liboqs_kem_ref"
BUILD_REF = REPO_ROOT / "scripts/build_liboqs_kem_ref.sh"

sys.path.insert(0, str(FIPS203_SE))
from golden_se_sampling import derand_bytes_from_seed  # noqa: E402

EK_BYTES = 1568
DK_BYTES = 3168
CT_BYTES = 1568
SS_BYTES = 32
KEM_SEED_BYTES = 64
M_BYTES = 32
SEED_D_DEFAULT = 20260619


def derand_z_from_seed(seed_d: int) -> bytes:
    """KeyGen 隐式拒绝秘密 z 的 host 派生（与 keygen 探针定点路径一致）。"""
    msg = f"exp-mlkem-f203-kem-k4:SEED_Z={seed_d}".encode()
    return hashlib.sha3_256(msg).digest()


def derand_m_from_seed(seed_d: int) -> bytes:
    """Encaps 消息种子 m 的 host 派生（定点路径；--random 不用）。"""
    msg = f"exp-mlkem-f203-kem-encaps-k4:SEED_M={seed_d}".encode()
    return hashlib.sha3_256(msg).digest()


def _ensure_ref() -> Path:
    """确保 liboqs ref 二进制存在；缺失则调用 build 脚本现编。"""
    if REF_BIN.is_file():
        return REF_BIN
    subprocess.check_call(["bash", str(BUILD_REF)])
    return REF_BIN


def _parse_hex(name: str, hex_str: str, n: int) -> bytes:
    raw = bytes.fromhex(hex_str)
    if len(raw) != n:
        raise SystemExit(f"[liboqs_kem_fixture] {name} want {n}B got {len(raw)}B")
    return raw


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--random",
        action="store_true",
        help="os.urandom 生成 kem_seed(64)=d‖z 与 m(32)，再调 liboqs derand（非定点 SEED_D）",
    )
    ap.add_argument(
        "--kem-seed-hex",
        default=os.environ.get("KEM_SEED_HEX", ""),
        help="可选：固定 64B kem_seed 的 hex（覆盖 --random 的 urandom；仍走 liboqs derand）",
    )
    ap.add_argument(
        "--m-hex",
        default=os.environ.get("M_HEX", ""),
        help="可选：固定 32B m 的 hex（覆盖 --random 的 urandom）",
    )
    ap.add_argument("--seed-d", type=int, default=int(os.environ.get("SEED_D", SEED_D_DEFAULT)))
    ap.add_argument("--out-dir", type=Path, required=True)
    args = ap.parse_args()

    out = args.out_dir
    out.mkdir(parents=True, exist_ok=True)

    if args.random or args.kem_seed_hex or args.m_hex:
        # --- 随机 / 显式字节模式：字节即 liboqs derand 输入 ---
        if args.kem_seed_hex:
            kem_seed = _parse_hex("kem_seed", args.kem_seed_hex, KEM_SEED_BYTES)
        elif args.random:
            kem_seed = os.urandom(KEM_SEED_BYTES)
        else:
            raise SystemExit(
                "[liboqs_kem_fixture] --m-hex 单独使用时须同时给 --kem-seed-hex 或 --random"
            )

        if args.m_hex:
            m = _parse_hex("m", args.m_hex, M_BYTES)
        elif args.random:
            m = os.urandom(M_BYTES)
        else:
            raise SystemExit(
                "[liboqs_kem_fixture] --kem-seed-hex 单独使用时须同时给 --m-hex 或 --random"
            )

        d, z = kem_seed[:32], kem_seed[32:]
        mode = "random/explicit"
        (out / "seed_d.bin").write_bytes(b"")  # 占位：本模式无 uint32 SEED_D
    else:
        # --- 定点 derand（旧 liboqs_kem_vs 路径）---
        d = derand_bytes_from_seed(args.seed_d)
        z = derand_z_from_seed(args.seed_d)
        m = derand_m_from_seed(args.seed_d)
        kem_seed = d + z
        mode = f"SEED_D={args.seed_d}"
        (out / "seed_d.bin").write_bytes(struct.pack("<I", args.seed_d))

    (out / "d.bin").write_bytes(d)
    (out / "z.bin").write_bytes(z)
    (out / "m.bin").write_bytes(m)
    (out / "kem_seed.bin").write_bytes(kem_seed)

    ref = _ensure_ref()

    ek_path = out / "ek_kem.bin"
    dk_path = out / "dk_kem.bin"
    subprocess.check_call([str(ref), "keygen", str(ek_path), str(dk_path), kem_seed.hex()])

    c_path = out / "c.bin"
    k_path = out / "K.bin"
    subprocess.check_call([str(ref), "encaps", str(ek_path), str(c_path), str(k_path), m.hex()])

    kdec_path = out / "K_decaps.bin"
    subprocess.check_call([str(ref), "decaps", str(dk_path), str(c_path), str(kdec_path)])

    c_bytes = bytearray(c_path.read_bytes())
    c_bytes[0] ^= 0x01
    c_bad_path = out / "c_bad.bin"
    c_bad_path.write_bytes(bytes(c_bytes))
    krej_path = out / "K_reject.bin"
    subprocess.check_call([str(ref), "decaps", str(dk_path), str(c_bad_path), str(krej_path)])

    k_enc = k_path.read_bytes()
    k_dec = kdec_path.read_bytes()
    k_rej = krej_path.read_bytes()
    j_zc = hashlib.shake_256(z + bytes(c_bytes)).digest(32)
    assert k_enc == k_dec, "[liboqs_kem_fixture] BUG: Encaps K != Decaps K'（fixture 自洽失败）"
    assert k_rej == j_zc, "[liboqs_kem_fixture] BUG: reject K != J(z||c_bad)（fixture 自洽失败）"
    assert k_rej != k_enc, "[liboqs_kem_fixture] BUG: 拒绝秘密与合法秘密相同"

    print(f"[liboqs_kem_fixture] mode={mode} -> {out}")
    print(f"[liboqs_kem_fixture] kem_seed={kem_seed.hex()[:16]}… m={m.hex()[:16]}…")
    print("[liboqs_kem_fixture] keygen/encaps/decaps/reject vectors OK (self-check passed)")


if __name__ == "__main__":
    main()
