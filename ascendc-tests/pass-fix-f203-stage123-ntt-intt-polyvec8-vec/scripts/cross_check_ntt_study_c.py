#!/usr/bin/env python3
"""
cross_check_ntt_study_c — 与 ntt_study C 参考实现对拍（独立脚本，不接入 run.sh）。

比对：
  1. golden_dst.bin（Python Tag5T 同构 golden）vs Tag5T C / F203 Tag3 C
  2. 若存在 output/dst.bin，额外比对 AscendC 设备输出 vs C 参考
  3. sepolyvec8_ntt_f203 交付 input0.bin / golden.bin vs C 参考

依赖：本机已安装 gcc/cmake；本目录 `thirdparty/ntt_study` 可编译。

用法（在探针目录或任意路径）：
  python3 scripts/cross_check_ntt_study_c.py
  F203_NTT_MODE=intt python3 scripts/cross_check_ntt_study_c.py
  python3 scripts/cross_check_ntt_study_c.py --mode both
  python3 scripts/cross_check_ntt_study_c.py --regen
  python3 scripts/cross_check_ntt_study_c.py --device   # 需先跑完 run.sh，且 F203_NTT_MODE 与 dst 一致
"""
from __future__ import annotations

import argparse
import os
import subprocess
import sys

import numpy as np

_SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
_CASE_DIR = os.path.normpath(os.path.join(_SCRIPT_DIR, ".."))
_NTT_STUDY = os.path.join(_CASE_DIR, "thirdparty/ntt_study")
_NTT_STUDY_BUILD = os.path.join(_NTT_STUDY, "build")
_DELIVER = os.path.join(_CASE_DIR, "thirdparty/ntt_study/deliverables/sepolyvec8_ntt_f203")
_C_SRC = os.path.join(_SCRIPT_DIR, "cross_check_ntt_study_ref.c")
_C_BIN = os.path.join(_CASE_DIR, "output", "cross_check_ntt_study_ref")

K = 8
N = 256


def _run(cmd: list[str], *, cwd: str | None = None, env: dict[str, str] | None = None) -> None:
    print(f"[cross_check] $ {' '.join(cmd)}")
    subprocess.run(cmd, cwd=cwd, env=env, check=True)


def ensure_ntt_study_built() -> None:
    os.makedirs(_NTT_STUDY_BUILD, exist_ok=True)
    if not os.path.isfile(os.path.join(_NTT_STUDY_BUILD, "CMakeCache.txt")):
        _run(["cmake", "..", "-DCMAKE_BUILD_TYPE=Release"], cwd=_NTT_STUDY_BUILD)
    _run(["cmake", "--build", ".", "--target", "ntt_study", "-j"], cwd=_NTT_STUDY_BUILD)


def collect_ntt_study_objects() -> list[str]:
    obj_root = os.path.join(_NTT_STUDY_BUILD, "CMakeFiles", "ntt_study.dir")
    objs: list[str] = []
    for root, _, files in os.walk(obj_root):
        for name in files:
            if name.endswith(".o") and name != "main.c.o":
                objs.append(os.path.join(root, name))
    if not objs:
        raise SystemExit(f"[cross_check] no ntt_study objects under {obj_root}; build failed?")
    return objs


def ensure_c_helper() -> str:
    ensure_ntt_study_built()
    os.makedirs(os.path.join(_CASE_DIR, "output"), exist_ok=True)
    inc = [
        "-I" + os.path.join(_NTT_STUDY, "include"),
        "-I" + os.path.join(_NTT_STUDY, "include/mlkem/stable"),
        "-I" + os.path.join(_NTT_STUDY, "include/mldsa/stable"),
        "-I" + os.path.join(_NTT_STUDY, "include/shared/stable"),
    ]
    cmd = ["gcc", "-O2", *inc, _C_SRC, *collect_ntt_study_objects(), "-lm", "-o", _C_BIN]
    _run(cmd)
    return _C_BIN


def cmp_via_c(bin_path: str, src: str, ref: str, mode: str) -> bool:
    proc = subprocess.run([bin_path, src, ref, mode, "--both"], check=False, text=True, capture_output=True)
    sys.stdout.write(proc.stdout)
    if proc.stderr:
        sys.stderr.write(proc.stderr)
    return proc.returncode == 0


def py_golden_vs_c(mode: str) -> bool:
    import importlib.util

    spec = importlib.util.spec_from_file_location("gen_data", os.path.join(_SCRIPT_DIR, "gen_data.py"))
    gen = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(gen)

    lut = gen.load_lut_t_i8(mode)
    src = np.fromfile(os.path.join(_CASE_DIR, "input", "src.bin"), dtype=np.int32).reshape(K, N)
    s0 = gen.encode_k8_s0(src)
    c_le, c_lo, c_he, c_ho = gen.mat_c_tmp_golden(s0, lut)
    mat_planar = gen.pack_mat_c_planar_k8(c_le, c_lo, c_he, c_ho)
    py_golden = gen.golden_dst_from_planar(mat_planar)

    tmp = os.path.join(_CASE_DIR, "output", f"_py_golden_{mode}.bin")
    py_golden.tofile(tmp)
    print(f"[cross_check] Python golden ({mode}) vs ntt_study C")
    return cmp_via_c(_C_BIN, os.path.join(_CASE_DIR, "input", "src.bin"), tmp, mode)


def maybe_device_vs_c(mode: str) -> bool | None:
    dst = os.path.join(_CASE_DIR, "output", "dst.bin")
    if not os.path.isfile(dst):
        print(f"[cross_check] skip device dst (missing {dst})")
        return None
    print(f"[cross_check] AscendC dst.bin ({mode}) vs ntt_study C")
    return cmp_via_c(_C_BIN, os.path.join(_CASE_DIR, "input", "src.bin"), dst, mode)


def deliverable_vs_c(mode: str) -> bool | None:
    if mode != "ntt":
        return None
    in0 = os.path.join(_DELIVER, "input0.bin")
    gold = os.path.join(_DELIVER, "golden.bin")
    if not (os.path.isfile(in0) and os.path.isfile(gold)):
        print(f"[cross_check] skip deliverable (missing {in0} or {gold})")
        return None
    print("[cross_check] sepolyvec8_ntt_f203 deliverable golden vs ntt_study C")
    return cmp_via_c(_C_BIN, in0, gold, mode)


def maybe_regen(mode: str) -> None:
    env = os.environ.copy()
    env["F203_NTT_MODE"] = mode
    _run([sys.executable, os.path.join(_SCRIPT_DIR, "gen_data.py")], cwd=_CASE_DIR, env=env)


def run_mode(mode: str, *, regen: bool, with_device: bool) -> bool:
    if regen:
        maybe_regen(mode)
    src = os.path.join(_CASE_DIR, "input", "src.bin")
    golden = os.path.join(_CASE_DIR, "output", "golden_dst.bin")
    if not os.path.isfile(src) or not os.path.isfile(golden):
        raise SystemExit(
            f"[cross_check] missing {src} or {golden}; run gen_data.py or pass --regen"
        )

    ok = True
    print(f"\n=== mode={mode} golden_dst.bin vs C ===")
    ok &= cmp_via_c(_C_BIN, src, golden, mode)

    print(f"\n=== mode={mode} Python recompute vs C ===")
    ok &= py_golden_vs_c(mode)

    if with_device:
        dev = maybe_device_vs_c(mode)
        if dev is not None:
            ok &= dev

    return ok


def main() -> None:
    ap = argparse.ArgumentParser(description="Cross-check vs ntt_study C reference (standalone)")
    ap.add_argument(
        "--mode",
        choices=("ntt", "intt", "both"),
        default=os.environ.get("F203_NTT_MODE", "both"),
        help="ntt / intt / both (default: F203_NTT_MODE or both)",
    )
    ap.add_argument(
        "--regen",
        action="store_true",
        help="regenerate input/golden via gen_data.py before compare",
    )
    ap.add_argument(
        "--device",
        action="store_true",
        help="also compare output/dst.bin (use with --mode ntt|intt, after matching run.sh)",
    )
    args = ap.parse_args()

    modes = ["ntt", "intt"] if args.mode == "both" else [args.mode]
    with_device = args.device and len(modes) == 1
    if args.device and not with_device:
        print("[cross_check] --device ignored unless --mode is ntt or intt (not both)")

    ensure_c_helper()

    ok = True
    for mode in modes:
        ok &= run_mode(mode, regen=args.regen, with_device=with_device)

    print("\n=== deliverable fixed input (NTT only) ===")
    d = deliverable_vs_c("ntt")
    if d is not None:
        ok &= d

    if ok:
        print("\n[cross_check] PASS")
    else:
        print("\n[cross_check] FAIL", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
