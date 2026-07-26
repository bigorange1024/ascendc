#!/usr/bin/env python3
# coding=utf-8
"""
kat_liboqs_vs_ascendc.py — liboqs PKE Encrypt ↔ AscendC 密文对拍驱动。

流水线位置：exp-fips203-mlkem-pke-encrypt-k4 的 **外部 KAT 批测**（非设备核本身）。
FIPS 203 / ML-KEM-1024：用 liboqs fixture 写 input/golden，再跑本用例 `run.sh`，
比较 `output/c.bin` 与 liboqs c（1568B）。与 golden 关系：此处 golden 来自 liboqs，
用于交叉验证设备 I/O，不替代 `scripts/gen_data.py` 的自包含 oracle。

流程（每轮）：
  1. liboqs_pke_fixture(SEED_D) → ek/m/coins/c
  2. prepare_kat_input（写 input + golden_v + golden/c，与 liboqs c 自检）
  3. ENCRYPT_KAT=1 bash run.sh（跳过 gen_data / golden cmp）
  4. output/c.bin 与 liboqs c 逐字节比

用法：
  bash kat_liboqs_vs_ascendc.sh
  KAT_CPU_COUNT=10 KAT_SIM_COUNT=1 bash kat_liboqs_vs_ascendc.sh
"""
from __future__ import annotations

import os
import secrets
import shutil
import subprocess
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
REPO_ROOT = ROOT.parent.parent.parent
sys.path.insert(0, str(REPO_ROOT / "scripts"))

from liboqs_pke_fixture import generate_fixture  # noqa: E402

CT_BYTES = 1568
SOC_VERSION = os.environ.get("KAT_SOC_VERSION", "Ascend910B4")
VERBOSE = os.environ.get("KAT_VERBOSE", "0") == "1"
LOG_PATH = Path(os.environ.get("KAT_LOG", str(ROOT / "output" / "kat_liboqs_vs_ascendc.log")))
FIXTURE_ROOT = Path(
    os.environ.get("KAT_FIXTURE_ROOT", str(ROOT / "output" / "liboqs_encrypt_kat"))
)


def _fail(label: str, round_no: int, total: int, seed_d: int, msg: str) -> None:
    raise SystemExit(f"[KAT FAIL] {label} round {round_no}/{total} seed_d={seed_d}: {msg}")


def random_seed_d() -> int:
    return secrets.randbelow(2**32)


def prepare_from_fixture(fix: Path) -> None:
    subprocess.check_call(
        [
            sys.executable,
            str(ROOT / "scripts" / "prepare_kat_input.py"),
            "--ek",
            str(fix / "ek_pke.bin"),
            "--m",
            str(fix / "m.bin"),
            "--coins",
            str(fix / "coins.bin"),
            "--c-ref",
            str(fix / "c.bin"),
            "--case-dir",
            str(ROOT),
        ],
        stdout=None if VERBOSE else subprocess.DEVNULL,
        stderr=None if VERBOSE else subprocess.DEVNULL,
    )


def run_ascendc_encrypt(seed_d: int, run_mode: str) -> np.ndarray:
    env = os.environ.copy()
    env["SEED_D"] = str(seed_d)
    env["ENCRYPT_KAT"] = "0" if VERBOSE else "1"
    env["ENCRYPT_SKIP_GEN_DATA"] = "1"
    # 首轮后可跳过重建（与 keygen KAT 同思路）
    cmd = ["bash", "run.sh", "-r", run_mode, "-v", SOC_VERSION]
    if VERBOSE:
        rc = subprocess.run(cmd, cwd=ROOT, env=env).returncode
    else:
        LOG_PATH.parent.mkdir(parents=True, exist_ok=True)
        with LOG_PATH.open("a", encoding="utf-8") as logf:
            logf.write(f"\n{'=' * 72}\n[kat] encrypt {run_mode} seed_d={seed_d}\n")
            logf.flush()
        rc = subprocess.run(
            cmd, cwd=ROOT, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
        ).returncode
    if rc != 0:
        raise SystemExit(f"run.sh({run_mode}) exit={rc} seed_d={seed_d} — see {LOG_PATH}")
    c = np.fromfile(ROOT / "output" / "c.bin", dtype=np.uint8)
    if c.size != CT_BYTES:
        raise SystemExit(f"encrypt output size c={c.size} seed_d={seed_d}")
    return c


def one_round(seed_d: int, run_mode: str, label: str, round_no: int, total: int) -> None:
    fix = FIXTURE_ROOT / str(seed_d)
    if fix.exists():
        shutil.rmtree(fix)
    generate_fixture(fix, seed_d)
    prepare_from_fixture(fix)
    c_dev = run_ascendc_encrypt(seed_d, run_mode)
    c_oqs = np.fromfile(fix / "c.bin", dtype=np.uint8)
    if not np.array_equal(c_dev, c_oqs):
        idx = int(np.argmax(c_dev != c_oqs))
        _fail(
            label,
            round_no,
            total,
            seed_d,
            f"c.bin @ {idx}: ascendc={c_dev.flat[idx]} liboqs={c_oqs.flat[idx]}",
        )


def parse_seed_list() -> tuple[list[int], list[int]]:
    cpu_count = int(os.environ.get("KAT_CPU_COUNT", "10"))
    sim_count = int(os.environ.get("KAT_SIM_COUNT", "1"))
    if cpu_count < 1 or sim_count < 1:
        raise SystemExit("KAT_CPU_COUNT and KAT_SIM_COUNT must be >= 1")
    raw = os.environ.get("KAT_SEEDS", "").strip()
    if raw:
        seeds = [int(x.strip()) for x in raw.split(",") if x.strip()]
        need = cpu_count + sim_count
        if len(seeds) < need:
            raise SystemExit(
                f"KAT_SEEDS has {len(seeds)} values, need {need} (CPU×{cpu_count} + SIM×{sim_count})"
            )
        return seeds[:cpu_count], seeds[cpu_count : cpu_count + sim_count]
    return [random_seed_d() for _ in range(cpu_count)], [random_seed_d() for _ in range(sim_count)]


def main() -> int:
    cpu_count = int(os.environ.get("KAT_CPU_COUNT", "10"))
    sim_count = int(os.environ.get("KAT_SIM_COUNT", "1"))
    if not VERBOSE:
        LOG_PATH.parent.mkdir(parents=True, exist_ok=True)
        LOG_PATH.write_text("[kat] exp-encrypt vs liboqs\n", encoding="utf-8")
        print(f"[kat] quiet — logs -> {LOG_PATH}")
    cpu_seeds, sim_seeds = parse_seed_list()
    with LOG_PATH.open("a", encoding="utf-8") as logf:
        logf.write(f"cpu seeds ({cpu_count}): {cpu_seeds}\nsim seeds ({sim_count}): {sim_seeds}\n")
    print(
        f"[kat] start CPU×{cpu_count} + SIM×{sim_count} "
        f"(liboqs fixture ek/m/coins/c ↔ exp-encrypt output/c)"
    )
    for i, s in enumerate(cpu_seeds):
        one_round(s, "cpu", "CPU", i + 1, cpu_count)
        print(f"[kat] CPU {i + 1}/{cpu_count} OK seed_d={s}", flush=True)
    print(f"[kat] CPU {cpu_count}/{cpu_count} OK", flush=True)
    for i, s in enumerate(sim_seeds):
        one_round(s, "sim", "SIM", i + 1, sim_count)
        print(f"[kat] SIM {i + 1}/{sim_count} OK seed_d={s}", flush=True)
    print(f"[kat] SIM {sim_count}/{sim_count} OK", flush=True)
    print(f"[kat] PASS CPU×{cpu_count} + SIM×{sim_count} (exp-encrypt vs liboqs)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
