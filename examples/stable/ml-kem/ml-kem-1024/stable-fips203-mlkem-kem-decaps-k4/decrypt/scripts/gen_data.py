#!/usr/bin/env python3
"""
gen_data.py — Alg.15 Decrypt 用例数据准备（Host 仅造夹具 + golden，不做设备密码学）。

生产契约（对齐 FIPS 203 Alg.15 / customspec）：
  input/  仅保留设备真正消费的文件：
    - dk_pke.bin (1536B)  — ByteEncode₁₂(ŝ)
    - c.bin (1568B)       — c₁‖c₂
    - lut_*_stacked.bin   — 静态 NTT/INTT LUT（与 seed 无关）
  output/
    - golden_m.bin (32B)  — Host oracle（DECRYPT_VERIFY=1）
    - m.bin               — 由 kernel 写出（本脚本不写）

夹具（仅用于派生 c，禁止当作生产输入落盘）：
  ek_pke / m / coins 写在 output/_gen_fixture/，用完不拷入 input/。
  SEED_D：定点可覆盖；默认 SHA3。m/coins：SHAKE256 扩字节。

背景：早期 gen_data 把 ek/m/coins/meta 也写进 input/，违反「仅必要 I/O」；
2026-07-09 收紧为 Alg.15 生产契约。
"""
from __future__ import annotations

import os
import shutil
import struct
import subprocess
import sys
from pathlib import Path

def _ascendc_repo_root(start: Path) -> Path:
    """自 start 向上查找含 AGENTS.md 与 scripts/ 的仓库根（兼容 ml-kem 参数组嵌套）。"""
    p = start.resolve()
    for d in [p, *p.parents]:
        if (d / "AGENTS.md").is_file() and (d / "scripts").is_dir():
            return d
    raise RuntimeError(f"cannot locate ascendc repo root from {start}")


import numpy as np

SCRIPT_DIR = Path(__file__).resolve().parent
CASE_DIR = SCRIPT_DIR.parent
HOST_GOLDEN = SCRIPT_DIR / "host_golden"
_REPO = _ascendc_repo_root(CASE_DIR)
sys.path.insert(0, str(_REPO / "library/shared/fips203_host_rng"))
from host_rng import expand_bytes, resolve_seed_d  # noqa: E402

DK_BYTES = 1536
CT_BYTES = 1568
MSG_BYTES = 32
_CASE_TAG = "fips203-mlkem-pke-decrypt-k4"

# 生产 input 允许的文件名（其余夹具不得残留）
_PROD_INPUT_NAMES = {
    "dk_pke.bin",
    "c.bin",
    "lut_even_stacked.bin",
    "lut_odd_stacked.bin",
    "lut_intt_even_stacked.bin",
    "lut_intt_odd_stacked.bin",
}


def _scrub_input(inp: Path) -> None:
    """删除 input/ 中非生产契约文件（如误留的 ek/m/coins/meta）。"""
    if not inp.is_dir():
        return
    for p in inp.iterdir():
        if p.is_file() and p.name not in _PROD_INPUT_NAMES:
            p.unlink()
            print(f"[gen_data] scrub non-prod input: {p.name}")


def main() -> None:
    seed_d, how = resolve_seed_d(_CASE_TAG)
    inp = CASE_DIR / "input"
    out = CASE_DIR / "output"
    fixture = out / "_gen_fixture"
    inp.mkdir(parents=True, exist_ok=True)
    out.mkdir(parents=True, exist_ok=True)
    if fixture.exists():
        shutil.rmtree(fixture)
    fixture.mkdir(parents=True, exist_ok=True)

    # --- 1) 私钥 dk（Alg.15 输入）---
    subprocess.run(
        [sys.executable, str(HOST_GOLDEN / "gen_dk_pke.py"), str(seed_d), str(inp / "dk_pke.bin")],
        check=True,
    )

    # --- 2) 夹具：ek + SHAKE m/coins → golden_c → 仅把 c 写入 input/ ---
    ek_path = fixture / "ek_pke.bin"
    m_path = fixture / "m.bin"
    coins_path = fixture / "coins.bin"
    subprocess.run(
        [sys.executable, str(HOST_GOLDEN / "gen_ek_pke.py"), str(seed_d), str(ek_path)],
        check=True,
    )
    np.frombuffer(expand_bytes(_CASE_TAG, "m", seed_d, MSG_BYTES), dtype=np.uint8).copy().tofile(m_path)
    np.frombuffer(expand_bytes(_CASE_TAG, "coins", seed_d, 32), dtype=np.uint8).copy().tofile(coins_path)
    subprocess.run(
        [
            sys.executable,
            str(HOST_GOLDEN / "golden_c.py"),
            str(ek_path),
            str(m_path),
            str(coins_path),
            str(inp / "c.bin"),
        ],
        check=True,
    )

    # meta 仅调试用，不进生产 input/
    (fixture / "meta.bin").write_bytes(struct.pack("<IIII", seed_d, DK_BYTES, CT_BYTES, MSG_BYTES))

    # --- 3) 静态 LUT（设备 NTT/INTT workspace）---
    subprocess.run([sys.executable, str(HOST_GOLDEN / "ntt_lut_bins.py"), str(inp)], check=True)

    # --- 4) Host golden m（对拍用，非设备输入）---
    if os.environ.get("DECRYPT_VERIFY", "1") == "1":
        subprocess.run(
            [
                sys.executable,
                str(HOST_GOLDEN / "golden_m.py"),
                str(inp / "dk_pke.bin"),
                str(inp / "c.bin"),
                str(out / "golden_m.bin"),
            ],
            check=True,
        )
        print(f"[gen_data] SEED_D={seed_d} via={how} golden_m OK")

    _scrub_input(inp)
    print(
        f"[gen_data] prod input = dk_pke + c + lut_* only; "
        f"fixture under {fixture.relative_to(CASE_DIR)}/"
    )


if __name__ == "__main__":
    main()
