#!/usr/bin/env python3
# coding=utf-8
"""
prepare_kat_input.py — 为 liboqs KAT / round-trip 准备 exp-decrypt 的 input/。

作用：把外部 dk_pke + c（及可选 liboqs m 参考）写成 Alg.15 生产契约：
  input/  — dk_pke.bin + c.bin + lut_*_stacked.bin（仅此）
  output/ — golden_m.bin（可选；供 run.sh DECRYPT_VERIFY=1 自检）

不调用 gen_data.py，避免覆盖外部 fixture 或把 ek/m/coins 写入 input/。
"""
from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

import numpy as np

_SCRIPT_DIR = Path(__file__).resolve().parent
_CASE_DIR = _SCRIPT_DIR.parent
_HOST_GOLDEN = _SCRIPT_DIR / "host_golden"

DK_BYTES = 1536
CT_BYTES = 1568
MSG_BYTES = 32

# 与 gen_data.py 一致：生产 input 允许的文件名
_PROD_INPUT_NAMES = {
    "dk_pke.bin",
    "c.bin",
    "lut_even_stacked.bin",
    "lut_odd_stacked.bin",
    "lut_intt_even_stacked.bin",
    "lut_intt_odd_stacked.bin",
}


def _scrub_input(inp: Path) -> None:
    """删除 input/ 中非生产契约文件（如误留的 meta/ek/m/coins）。"""
    if not inp.is_dir():
        return
    for p in inp.iterdir():
        if p.is_file() and p.name not in _PROD_INPUT_NAMES:
            p.unlink()
            print(f"[prepare_kat] scrub non-prod input: {p.name}")


def _gen_lut_bins(inp: Path) -> None:
    subprocess.run(
        [sys.executable, str(_HOST_GOLDEN / "ntt_lut_bins.py"), str(inp)],
        check=True,
    )


def _host_golden_m(dk: bytes, c: bytes) -> bytes:
    """Host oracle m（与 golden_m.py 同语义）。"""
    import tempfile

    with tempfile.TemporaryDirectory() as td:
        tdp = Path(td)
        (tdp / "dk.bin").write_bytes(dk)
        (tdp / "c.bin").write_bytes(c)
        out_m = tdp / "m.bin"
        subprocess.run(
            [
                sys.executable,
                str(_HOST_GOLDEN / "golden_m.py"),
                str(tdp / "dk.bin"),
                str(tdp / "c.bin"),
                str(out_m),
            ],
            check=True,
        )
        return out_m.read_bytes()


def prepare(
    dk_path: Path,
    c_path: Path,
    m_ref_path: Path | None = None,
    case_dir: Path | None = None,
    write_golden: bool = True,
) -> None:
    """
    写入 case_dir/input（及可选 output/golden_m.bin）。

    @param dk_path / c_path 外部字节源（liboqs fixture 或 device KeyGen/Encrypt）
    @param m_ref_path 若给：与 host golden_m 自检后写入 golden_m.bin
    @param write_golden False 时仅写 input（round-trip 由 roundtrip_pke_verify 对拍）
    """
    root = (case_dir or _CASE_DIR).resolve()
    inp = root / "input"
    out = root / "output"
    inp.mkdir(parents=True, exist_ok=True)
    out.mkdir(parents=True, exist_ok=True)

    dk = dk_path.read_bytes()
    c = c_path.read_bytes()
    if len(dk) != DK_BYTES or len(c) != CT_BYTES:
        raise SystemExit(f"[prepare_kat] bad sizes dk={len(dk)} c={len(c)}")

    shutil.copy2(dk_path, inp / "dk_pke.bin")
    shutil.copy2(c_path, inp / "c.bin")
    _gen_lut_bins(inp)
    _scrub_input(inp)

    if not write_golden:
        print(f"[prepare_kat] OK dk+c+lut → {inp} (no golden_m)")
        return

    m_host = _host_golden_m(dk, c)
    if len(m_host) != MSG_BYTES:
        raise SystemExit(f"[prepare_kat] host m size {len(m_host)}")
    if m_ref_path is not None:
        m_ref = m_ref_path.read_bytes()
        if m_ref != m_host:
            raise SystemExit("[prepare_kat] host golden_m != 外部 m 参考（liboqs/fixture）")
        (out / "golden_m.bin").write_bytes(m_ref)
        src = "external"
    else:
        (out / "golden_m.bin").write_bytes(m_host)
        src = "host"
    print(f"[prepare_kat] OK golden_m={src} dk+c+lut → {inp}")


def main() -> None:
    ap = argparse.ArgumentParser(description="exp-decrypt KAT/roundtrip input 准备")
    ap.add_argument("--dk", type=Path, required=True)
    ap.add_argument("--c", type=Path, required=True)
    ap.add_argument("--m-ref", type=Path, default=None, help="可选：liboqs m.bin 作 golden 并自检")
    ap.add_argument("--case-dir", type=Path, default=_CASE_DIR)
    ap.add_argument(
        "--no-golden",
        action="store_true",
        help="round-trip：仅写 input，不写 golden_m（验收取 roundtrip_pke_verify）",
    )
    args = ap.parse_args()
    prepare(
        args.dk.resolve(),
        args.c.resolve(),
        args.m_ref.resolve() if args.m_ref else None,
        args.case_dir.resolve(),
        write_golden=not args.no_golden,
    )


if __name__ == "__main__":
    main()
