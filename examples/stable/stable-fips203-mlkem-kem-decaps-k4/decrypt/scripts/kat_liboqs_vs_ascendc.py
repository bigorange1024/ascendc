#!/usr/bin/env python3
# coding=utf-8
"""
kat_liboqs_vs_ascendc.py — liboqs PKE Decrypt ↔ 本 stable Decrypt（1-kernel fused）KAT。

## 目的
对齐 FIPS 203 Alg.15：fixture 的 dk+c → 设备 m，与 liboqs m 逐字节对拍。

## 每轮流程
1. liboqs fixture(SEED_D) → dk/c/m
2. prepare_kat_input
3. DECRYPT_KAT=1 bash run.sh
4. 比较 output/m.bin 与 fixture m.bin

用法：bash kat_liboqs_vs_ascendc.sh；KAT_CPU_COUNT / KAT_SIM_COUNT / KAT_SEEDS。
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

from liboqs_pke_decrypt_fixture import generate_fixture  # noqa: E402

MSG_BYTES = 32
SOC_VERSION = os.environ.get("KAT_SOC_VERSION", "Ascend910B4")
VERBOSE = os.environ.get("KAT_VERBOSE", "0") == "1"
LOG_PATH = Path(os.environ.get("KAT_LOG", str(ROOT / "output" / "kat_liboqs_vs_ascendc.log")))
FIXTURE_ROOT = Path(
    os.environ.get("KAT_FIXTURE_ROOT", str(ROOT / "output" / "liboqs_decrypt_kat"))
)


def _fail(label: str, round_no: int, total: int, seed_d: int, msg: str) -> None:
    """
    打印 KAT 失败信息并 SystemExit。
    """
    raise SystemExit(f"[KAT FAIL] {label} round {round_no}/{total} seed_d={seed_d}: {msg}")


def random_seed_d() -> int:
    """
    均匀随机 32-bit SEED_D。
    """
    return secrets.randbelow(2**32)


def prepare_from_fixture(fix: Path) -> None:
    """
    fixture 的 dk/c/m → 用例 input/ + golden_m。
    """
    subprocess.check_call(
        [
            sys.executable,
            str(ROOT / "scripts" / "prepare_kat_input.py"),
            "--dk",
            str(fix / "dk_pke.bin"),
            "--c",
            str(fix / "c.bin"),
            "--m-ref",
            str(fix / "m.bin"),
            "--case-dir",
            str(ROOT),
        ],
        stdout=None if VERBOSE else subprocess.DEVNULL,
        stderr=None if VERBOSE else subprocess.DEVNULL,
    )


def run_ascendc_decrypt(seed_d: int, run_mode: str) -> np.ndarray:
    """
    DECRYPT_KAT 模式下跑 run.sh，读回 output/m.bin。
    """
    env = os.environ.copy()
    env["SEED_D"] = str(seed_d)
    env["DECRYPT_KAT"] = "0" if VERBOSE else "1"
    env["DECRYPT_SKIP_GEN_DATA"] = "1"
    cmd = ["bash", "run.sh", "-r", run_mode, "-v", SOC_VERSION]
    if VERBOSE:
        rc = subprocess.run(cmd, cwd=ROOT, env=env).returncode
    else:
        LOG_PATH.parent.mkdir(parents=True, exist_ok=True)
        with LOG_PATH.open("a", encoding="utf-8") as logf:
            logf.write(f"\n{'=' * 72}\n[kat] decrypt {run_mode} seed_d={seed_d}\n")
            logf.flush()
        rc = subprocess.run(
            cmd, cwd=ROOT, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
        ).returncode
    if rc != 0:
        raise SystemExit(f"run.sh({run_mode}) exit={rc} seed_d={seed_d} — see {LOG_PATH}")
    m = np.fromfile(ROOT / "output" / "m.bin", dtype=np.uint8)
    if m.size != MSG_BYTES:
        raise SystemExit(f"decrypt output size m={m.size} seed_d={seed_d}")
    return m


def one_round(seed_d: int, run_mode: str, label: str, round_no: int, total: int) -> None:
    """
    单轮：生成 fixture → 准备 input → 跑设备 → 与 fixture m 比较。
    """
    fix = FIXTURE_ROOT / str(seed_d)
    if fix.exists():
        shutil.rmtree(fix)
    generate_fixture(fix, seed_d)
    prepare_from_fixture(fix)
    m_dev = run_ascendc_decrypt(seed_d, run_mode)
    m_oqs = np.fromfile(fix / "m.bin", dtype=np.uint8)
    if not np.array_equal(m_dev, m_oqs):
        idx = int(np.argmax(m_dev != m_oqs))
        _fail(
            label,
            round_no,
            total,
            seed_d,
            f"m.bin @ {idx}: ascendc={m_dev.flat[idx]} fixture={m_oqs.flat[idx]}",
        )


def parse_seed_list() -> tuple[list[int], list[int]]:
    """
    解析 KAT_SEEDS 或随机生成 CPU/SIM 种子列表。
    """
    cpu_count = int(os.environ.get("KAT_CPU_COUNT", "10"))
    sim_count = int(os.environ.get("KAT_SIM_COUNT", "1"))
    if cpu_count < 1 or sim_count < 0:
        raise SystemExit("KAT_CPU_COUNT must be >= 1 and KAT_SIM_COUNT must be >= 0")
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
    """
    按 KAT_CPU_COUNT / KAT_SIM_COUNT 跑多轮对拍；成功返回 0。
    """
    cpu_count = int(os.environ.get("KAT_CPU_COUNT", "10"))
    sim_count = int(os.environ.get("KAT_SIM_COUNT", "1"))
    if not VERBOSE:
        LOG_PATH.parent.mkdir(parents=True, exist_ok=True)
        LOG_PATH.write_text("[kat] stable-decrypt vs fixture\n", encoding="utf-8")
        print(f"[kat] quiet — logs -> {LOG_PATH}")
    cpu_seeds, sim_seeds = parse_seed_list()
    with LOG_PATH.open("a", encoding="utf-8") as logf:
        logf.write(f"cpu seeds ({cpu_count}): {cpu_seeds}\nsim seeds ({sim_count}): {sim_seeds}\n")
    print(
        f"[kat] start CPU×{cpu_count}"
        + (f" + SIM×{sim_count}" if sim_count else "")
        + " (liboqs keygen + host c → dk/c/m ↔ stable-decrypt output/m)"
    )
    for i, s in enumerate(cpu_seeds):
        one_round(s, "cpu", "CPU", i + 1, cpu_count)
        print(f"[kat] CPU {i + 1}/{cpu_count} OK seed_d={s}", flush=True)
    print(f"[kat] CPU {cpu_count}/{cpu_count} OK", flush=True)
    if sim_count > 0:
        for i, s in enumerate(sim_seeds):
            one_round(s, "sim", "SIM", i + 1, sim_count)
            print(f"[kat] SIM {i + 1}/{sim_count} OK seed_d={s}", flush=True)
        print(f"[kat] SIM {sim_count}/{sim_count} OK", flush=True)
        print(f"[kat] PASS CPU×{cpu_count} + SIM×{sim_count} (stable-decrypt vs fixture)")
    else:
        print(f"[kat] PASS CPU×{cpu_count} (stable-decrypt vs fixture)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
