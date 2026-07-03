#!/usr/bin/env python3
"""
liboqs_kem_keygen_fixture.py — 为 KEM.KeyGen 旁路 A 正确性测试生成参考向量。

与 liboqs_kem_fixture.py 的区别：
- liboqs_kem_fixture.py：d/z 由 SEED_D 经域分离 SHA3 派生（对齐设备默认生产路径）。
- 本脚本：kem_seed = d(32)‖z(32) 直接来自 **os.urandom**（或指定文件），不经 SEED_D 派生。
  目的是让 liboqs keypair_derand 与 keygen 探针的旁路 A（KEM_KG_EXT_SEED=1，
  prep 取前 32B 作 d、finish 取后 32B 作 z）吃**逐字节相同的随机字节**，
  从而在任意（非 SHA3 像）随机性上验证 KeyGen 核实现正确性（类似 NIST KAT 语义）。

产出（写入 --out-dir）：
- kem_seed.bin (64B)  ：本轮随机 d‖z（探针 input/kem_seed.bin 亦取此文件）
- ek_kem.bin (1568B)  ：liboqs keypair_derand 参考公钥
- dk_kem.bin (3168B)  ：liboqs keypair_derand 参考私钥（展开布局 dk_pke‖ek‖H(ek)‖z）
"""
from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
REF_BIN = REPO_ROOT / "scripts/liboqs_kem_ref"
BUILD_REF = REPO_ROOT / "scripts/build_liboqs_kem_ref.sh"

KEM_SEED_BYTES = 64  # d(32) || z(32)
EK_BYTES = 1568
DK_BYTES = 3168


def _ensure_ref() -> Path:
    """确保 liboqs ref 二进制存在；缺失则调用 build 脚本现编。"""
    if REF_BIN.is_file():
        return REF_BIN
    subprocess.check_call(["bash", str(BUILD_REF)])
    return REF_BIN


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out-dir", type=Path, required=True)
    ap.add_argument(
        "--kem-seed-file",
        type=Path,
        default=None,
        help="指定 64B kem_seed 文件；缺省用 os.urandom(64) 随机生成",
    )
    args = ap.parse_args()

    # --- 取随机字节（旁路 A 的“共享随机源”）---
    if args.kem_seed_file is not None:
        kem_seed = args.kem_seed_file.read_bytes()
        if len(kem_seed) != KEM_SEED_BYTES:
            print(f"[kg_fixture] FAIL kem_seed 长度 {len(kem_seed)} != {KEM_SEED_BYTES}", file=sys.stderr)
            return 1
    else:
        kem_seed = os.urandom(KEM_SEED_BYTES)

    out = args.out_dir
    out.mkdir(parents=True, exist_ok=True)
    (out / "kem_seed.bin").write_bytes(kem_seed)

    ref = _ensure_ref()

    # --- liboqs keypair_derand(kem_seed) → 参考 ek/dk ---
    ek_path = out / "ek_kem.bin"
    dk_path = out / "dk_kem.bin"
    subprocess.check_call([str(ref), "keygen", str(ek_path), str(dk_path), kem_seed.hex()])

    # 尺寸自洽（liboqs 语义异常早暴露）
    assert ek_path.stat().st_size == EK_BYTES, "[kg_fixture] BUG: ek_kem 尺寸异常"
    assert dk_path.stat().st_size == DK_BYTES, "[kg_fixture] BUG: dk_kem 尺寸异常"

    print(f"[kg_fixture] kem_seed(os.urandom)={kem_seed.hex()[:16]}… -> {out} (ek/dk ready)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
