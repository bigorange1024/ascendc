#!/usr/bin/env python3
# coding=utf-8
"""Golden 入口：读本探针 fixtures/input，写 output/golden_*.bin。

流水线位置（Alg.14 Encrypt prep，行 3–15）：
  1. 安装 fixtures/ek_pke.bin → input/ek_pke.bin（1568B；ρ 在偏移 1536）
  2. 由 COINS_SEED 派生 coins[32] → input/coins.bin
  3. ρ → build_a_hat_from_rho → golden_a_hat.bin（16×256 int32）
  4. coins → build_re_from_coins → golden_re.bin（9×256 int32）
  5. 另写 golden_rho.bin（调试用，设备不对拍）

自包含：仅 import 本目录 scripts/golden_encrypt_prep.py 与 scripts/prep/alg7_geom.py。
禁止 import 其它 ascendc-tests 用例或 library/shared。

与设备关系：本脚本是黑盒 oracle；AscendC 只需 I/O 与 golden 一致，不必同构实现。
"""
from __future__ import annotations

import hashlib
import os
import shutil
from pathlib import Path


from golden_encrypt_prep import XOF_BYTES, build_a_hat_from_rho, build_re_from_coins

ROOT = Path(__file__).resolve().parent.parent
FIXTURES_EK = ROOT / "fixtures" / "ek_pke.bin"
INPUT_EK = ROOT / "input" / "ek_pke.bin"

# 默认可复现 coins；可用环境变量 COINS_SEED 覆盖（调试对照须显式指定）
COINS_SEED_DEFAULT = 20260706
# ML-KEM-1024 ek_PKE 编码长度；尾 32B 为公共种子 ρ
EK_PKE_BYTES = 1568
RHO_OFFSET = 1536
RHO_BYTES = 32


def coins_from_seed(coins_seed: int) -> bytes:
    """由整数种子派生确定性 coins[32]（SHA3-256 域分离字符串）。

    与设备无关：仅保证 gen_data / 多次跑用例可复现同一 coins.bin。
    """
    msg = f"fix-f203-alg14-encrypt-prep:COINS_SEED={coins_seed}".encode()
    return hashlib.sha3_256(msg).digest()


def install_ek_pke(input_dir: Path) -> bytes:
    """将 fixtures/ek_pke.bin 安装到 input/；缺失则失败（不引用 stable 路径）。

    背景：探针自包含，一次性从 KeyGen 产物复制到 fixtures/ 后，本脚本只读本地 fixtures。
    @return 完整 ek_pke 字节（1568B）
    """
    if not FIXTURES_EK.is_file() or FIXTURES_EK.stat().st_size != EK_PKE_BYTES:
        raise SystemExit(
            f"missing fixtures/ek_pke.bin ({EK_PKE_BYTES}B); "
            f"一次性从 stable KeyGen output 复制到 {FIXTURES_EK}"
        )
    input_dir.mkdir(parents=True, exist_ok=True)
    dst = input_dir / "ek_pke.bin"
    # 已存在且尺寸正确则复用，避免每次覆盖
    if not dst.is_file() or dst.stat().st_size != EK_PKE_BYTES:
        shutil.copy2(FIXTURES_EK, dst)
    ek = dst.read_bytes()
    if len(ek) != EK_PKE_BYTES:
        raise SystemExit(f"ek_pke size {len(ek)}")
    return ek


def main() -> None:
    """生成 input/coins.bin 与 output/golden_{a_hat,re,rho}.bin。"""
    coins_seed = int(os.environ.get("COINS_SEED", str(COINS_SEED_DEFAULT)))
    coins = coins_from_seed(coins_seed)

    input_dir = ROOT / "input"
    output_dir = ROOT / "output"
    output_dir.mkdir(exist_ok=True)

    # 行 3–7：从 ek 尾取 ρ，SampleNTT 得 Â
    ek = install_ek_pke(input_dir)
    rho = ek[RHO_OFFSET : RHO_OFFSET + RHO_BYTES]

    a_hat = build_a_hat_from_rho(rho)
    # 行 8–15：coins → PRF+CBD → r‖e1‖e2
    re = build_re_from_coins(coins)

    (input_dir / "coins.bin").write_bytes(coins)
    a_hat.tofile(output_dir / "golden_a_hat.bin")
    re.tofile(output_dir / "golden_re.bin")
    (output_dir / "golden_rho.bin").write_bytes(rho)

    print(
        f"[gen_data] ek_pke from fixtures→input ({EK_PKE_BYTES}B) "
        f"rho@{RHO_OFFSET} COINS_SEED={coins_seed} XOF_BYTES={XOF_BYTES} "
        f"a_hat={a_hat.shape} re={re.shape}"
    )


if __name__ == "__main__":
    main()
