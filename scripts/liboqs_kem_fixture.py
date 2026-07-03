#!/usr/bin/env python3
"""
liboqs_kem_fixture.py — 生成 liboqs KEM 全链黑盒向量（ml_kem_1024 / k=4）。

覆盖 KeyGen / Encaps / Decaps 三阶段 + 隐式拒绝路径，供 `liboqs_kem_vs_ascendc.sh`
逐级与 AscendC 探针输出对拍。

device 可复现派生（与三个 KEM 探针一致，前缀严禁漂移）：
- d = SHA3-256("exp-mlkem-f203-2s1e-k4:SEED_D={seed}")        （golden_se_sampling.derand_bytes_from_seed）
- z = SHA3-256("exp-mlkem-f203-kem-k4:SEED_Z={seed}")          （keygen 探针，本文件 derand_z_from_seed）
- m = SHA3-256("exp-mlkem-f203-kem-encaps-k4:SEED_M={seed}")   （encaps 探针 gen_data.derand_m_from_seed）

产出（写入 --out-dir）：
- seed_d.bin / d.bin / z.bin / kem_seed.bin / m.bin  ：种子与派生输入（调试可读，不进探针生产 run.sh）
- ek_kem.bin (1568B) / dk_kem.bin (3168B)            ：KeyGen 参考公私钥
- c.bin (1568B) / K.bin (32B)                        ：Encaps(ek, m) 参考密文与共享秘密
- K_decaps.bin (32B)                                 ：Decaps(dk, c) 参考共享秘密（合法路径应 == K.bin）
- c_bad.bin (1568B) / K_reject.bin (32B)             ：篡改 c 一字节 + Decaps(dk, c_bad) 参考隐式拒绝秘密 J(z‖c_bad)
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
SEED_D_DEFAULT = 20260619


def derand_z_from_seed(seed_d: int) -> bytes:
    """KeyGen 隐式拒绝秘密 z 的 host 派生（与 keygen 探针一致）。"""
    msg = f"exp-mlkem-f203-kem-k4:SEED_Z={seed_d}".encode()
    return hashlib.sha3_256(msg).digest()


def derand_m_from_seed(seed_d: int) -> bytes:
    """Encaps 消息种子 m 的 host 派生（与 encaps 探针 gen_data.derand_m_from_seed 逐字一致）。"""
    msg = f"exp-mlkem-f203-kem-encaps-k4:SEED_M={seed_d}".encode()
    return hashlib.sha3_256(msg).digest()


def _ensure_ref() -> Path:
    """确保 liboqs ref 二进制存在；缺失则调用 build 脚本现编。"""
    if REF_BIN.is_file():
        return REF_BIN
    subprocess.check_call(["bash", str(BUILD_REF)])
    return REF_BIN


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed-d", type=int, default=int(os.environ.get("SEED_D", SEED_D_DEFAULT)))
    ap.add_argument("--out-dir", type=Path, required=True)
    args = ap.parse_args()

    # --- 派生输入种子 ---
    d = derand_bytes_from_seed(args.seed_d)
    z = derand_z_from_seed(args.seed_d)
    m = derand_m_from_seed(args.seed_d)
    kem_seed = d + z  # keypair_derand 吃 64B = d||z

    out = args.out_dir
    out.mkdir(parents=True, exist_ok=True)
    (out / "seed_d.bin").write_bytes(struct.pack("<I", args.seed_d))
    (out / "d.bin").write_bytes(d)
    (out / "z.bin").write_bytes(z)
    (out / "m.bin").write_bytes(m)
    (out / "kem_seed.bin").write_bytes(kem_seed)

    ref = _ensure_ref()

    # --- Phase 1: KeyGen (kem_seed = d||z) → ek/dk ---
    ek_path = out / "ek_kem.bin"
    dk_path = out / "dk_kem.bin"
    subprocess.check_call([str(ref), "keygen", str(ek_path), str(dk_path), kem_seed.hex()])

    # --- Phase 2: Encaps(ek, m) → c/K ---
    c_path = out / "c.bin"
    k_path = out / "K.bin"
    subprocess.check_call([str(ref), "encaps", str(ek_path), str(c_path), str(k_path), m.hex()])

    # --- Phase 3: Decaps(dk, c) → K'（合法路径，应与 Encaps K 一致）---
    kdec_path = out / "K_decaps.bin"
    subprocess.check_call([str(ref), "decaps", str(dk_path), str(c_path), str(kdec_path)])

    # --- Phase 4: 拒绝路径。篡改密文 c 首字节 → Decaps 触发隐式拒绝返回 J(z‖c_bad) ---
    #   注意：这里篡改的是「输入密文 c」本身（区别于 device KEM_DECAPS_TAMPER_C=1 改内部 coins）。
    #   liboqs decaps 对被篡改 c 会重加密失配 → 返回 SHAKE256(z‖c_bad, 32)，作为拒绝参考秘密。
    c_bytes = bytearray(c_path.read_bytes())
    c_bytes[0] ^= 0x01
    c_bad_path = out / "c_bad.bin"
    c_bad_path.write_bytes(bytes(c_bytes))
    krej_path = out / "K_reject.bin"
    subprocess.check_call([str(ref), "decaps", str(dk_path), str(c_bad_path), str(krej_path)])

    # 自洽校验（fixture 内部即抓 liboqs 语义异常）：合法路径 K==K'，拒绝路径 K_reject==J(z‖c_bad)。
    k_enc = k_path.read_bytes()
    k_dec = kdec_path.read_bytes()
    k_rej = krej_path.read_bytes()
    j_zc = hashlib.shake_256(z + bytes(c_bytes)).digest(32)
    assert k_enc == k_dec, "[liboqs_kem_fixture] BUG: Encaps K != Decaps K'（fixture 自洽失败）"
    assert k_rej == j_zc, "[liboqs_kem_fixture] BUG: reject K != J(z||c_bad)（fixture 自洽失败）"
    assert k_rej != k_enc, "[liboqs_kem_fixture] BUG: 拒绝秘密与合法秘密相同"

    print(f"[liboqs_kem_fixture] SEED_D={args.seed_d} -> {out}")
    print("[liboqs_kem_fixture] keygen/encaps/decaps/reject vectors OK (self-check passed)")


if __name__ == "__main__":
    main()
