#!/usr/bin/env python3
# @exp exp-mlkem-f203-pke-keygen-k4
# coding=utf-8
"""liboqs PKE KeyGen ↔ AscendC 对拍：默认 10×CPU + 1×SIM，每轮同 SEED_D 随机种子。

种子契约（与 run.sh / 设备 DerandFromSeedD 一致）：
  SEED_D (uint32) → d = SHA3-256("exp-mlkem-f203-2s1e-k4:SEED_D=<decimal>")
  liboqs：d 作为 ml_kem_1024 indcpa coins[0:32]
  AscendC：环境变量 SEED_D 传入 run.sh 生产 2-Launch 全链

对拍字节：ek_PKE 1568 B；dk_PKE 1536 B

用法：
  bash kat_liboqs_vs_ascendc.sh
  KAT_VERBOSE=1 bash kat_liboqs_vs_ascendc.sh   # 完整 run.sh 日志打到控制台

环境变量：
  KAT_CPU_COUNT  — CPU 轮数，默认 10
  KAT_SIM_COUNT  — SIM 轮数，默认 1
  KAT_VERBOSE    — 1=不静默 run.sh（默认 0）
  KAT_LOG        — 静默时 run.sh 日志（默认 output/kat_liboqs_vs_ascendc.log）
  KAT_PROGRESS   — 进度打印间隔（默认 1）
"""
from __future__ import annotations

import os
import secrets
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
REPO_ROOT = ROOT.parents[2]
FIPS203_SE_SCRIPTS = REPO_ROOT / "library" / "shared" / "fips203_se_sample"
sys.path.insert(0, str(FIPS203_SE_SCRIPTS))
sys.path.insert(0, str(ROOT / "scripts"))

from golden_se_sampling import derand_bytes_from_seed  # noqa: E402

EK_PKE_BYTES = 1568
DK_PKE_BYTES = 1536
REF_BIN = ROOT / "scripts" / "liboqs_pke_keygen_ref"
BUILD_REF_SH = ROOT / "scripts" / "build_liboqs_pke_ref.sh"
SOC_VERSION = os.environ.get("KAT_SOC_VERSION", "Ascend910B4")
VERBOSE = os.environ.get("KAT_VERBOSE", "0") == "1"
LOG_PATH = Path(os.environ.get("KAT_LOG", str(ROOT / "output" / "kat_liboqs_vs_ascendc.log")))
PROGRESS_EVERY = max(1, int(os.environ.get("KAT_PROGRESS", "1")))


def _log_run_header(run_mode: str, seed_d: int, logf) -> None:
    logf.write(f"\n{'=' * 72}\n")
    logf.write(f"[kat] {run_mode} seed_d={seed_d}\n")
    logf.flush()


def _tail_log(path: Path, lines: int = 40) -> str:
    if not path.is_file():
        return "(no log file)"
    text = path.read_text(errors="replace").splitlines()
    return "\n".join(text[-lines:])


def print_sim_metrics_console(log_path: Path, case_dir: Path | None = None) -> None:
    root = case_dir or ROOT
    from parse_keygen_sim_metrics import build_summary, format_summary_lines  # noqa: WPS433

    summary = build_summary(root, log_path)
    for line in format_summary_lines(summary):
        print(f"[kat] {line.replace('[keygen] ', '', 1)}", flush=True)


def _ensure_liboqs_ref() -> Path:
    if REF_BIN.is_file():
        return REF_BIN
    kw = {} if VERBOSE else {"stdout": subprocess.DEVNULL, "stderr": subprocess.DEVNULL}
    subprocess.check_call(["bash", str(BUILD_REF_SH)], **kw)
    return REF_BIN


def random_seed_d() -> int:
    return secrets.randbelow(2**32)


def liboqs_pke_keygen(d: bytes) -> tuple[np.ndarray, np.ndarray]:
    ref = _ensure_liboqs_ref()
    with tempfile.TemporaryDirectory(prefix="oqs_kat_") as td:
        td_path = Path(td)
        ek_path = td_path / "ek_oqs.bin"
        dk_path = td_path / "dk_oqs.bin"
        subprocess.check_call(
            [str(ref), str(ek_path), str(dk_path), d.hex()],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        ek = np.fromfile(ek_path, dtype=np.uint8)
        dk = np.fromfile(dk_path, dtype=np.uint8)
    if ek.size != EK_PKE_BYTES or dk.size != DK_PKE_BYTES:
        raise SystemExit(f"liboqs size ek={ek.size} dk={dk.size}")
    return ek, dk


def run_ascendc_keygen(seed_d: int, run_mode: str, skip_build: bool) -> tuple[np.ndarray, np.ndarray]:
    if run_mode == "sim":
        skip_build = False
    env = os.environ.copy()
    env["SEED_D"] = str(seed_d)
    env["KEYGEN_KAT"] = "1"
    if run_mode == "sim":
        env["KEYGEN_KERNEL_BUDGET_SEC"] = os.environ.get("KEYGEN_KERNEL_BUDGET_SEC", "1200")
    if skip_build:
        env["KEYGEN_SKIP_REBUILD"] = "1"
    else:
        env.pop("KEYGEN_SKIP_REBUILD", None)

    cmd = ["bash", "run.sh", "-r", run_mode, "-v", SOC_VERSION]
    if VERBOSE:
        rc = subprocess.run(cmd, cwd=ROOT, env=env).returncode
    else:
        LOG_PATH.parent.mkdir(parents=True, exist_ok=True)
        with LOG_PATH.open("a", encoding="utf-8") as logf:
            _log_run_header(run_mode, seed_d, logf)
            logf.flush()
        rc = subprocess.run(
            cmd,
            cwd=ROOT,
            env=env,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        ).returncode

    if rc != 0:
        msg = f"[kat] run.sh({run_mode}) exit={rc} seed_d={seed_d}"
        if not VERBOSE:
            msg += f" — see {LOG_PATH}\n{_tail_log(LOG_PATH)}"
        raise SystemExit(msg)

    ek = np.fromfile(ROOT / "output" / "ek_pke.bin", dtype=np.uint8)
    dk = np.fromfile(ROOT / "output" / "dk_pke.bin", dtype=np.uint8)
    if ek.size != EK_PKE_BYTES or dk.size != DK_PKE_BYTES:
        if not VERBOSE:
            print(_tail_log(LOG_PATH), file=sys.stderr)
        raise SystemExit(f"ascendc({run_mode}) output size ek={ek.size} dk={dk.size}")
    return ek, dk


def compare_pke(
    seed_d: int,
    label: str,
    ek_oqs: np.ndarray,
    dk_oqs: np.ndarray,
    ek_dev: np.ndarray,
    dk_dev: np.ndarray,
) -> None:
    if not np.array_equal(ek_dev, ek_oqs):
        idx = int(np.argmax(ek_dev != ek_oqs))
        raise SystemExit(
            f"[KAT FAIL] {label} seed_d={seed_d} ek_pke @ {idx}: "
            f"ascendc={ek_dev.flat[idx]} liboqs={ek_oqs.flat[idx]} — see {LOG_PATH}"
        )
    if not np.array_equal(dk_dev, dk_oqs):
        idx = int(np.argmax(dk_dev != dk_oqs))
        raise SystemExit(
            f"[KAT FAIL] {label} seed_d={seed_d} dk_pke @ {idx}: "
            f"ascendc={dk_dev.flat[idx]} liboqs={dk_oqs.flat[idx]} — see {LOG_PATH}"
        )


def _print_progress(label: str, idx: int, total: int) -> None:
    if (idx + 1) % PROGRESS_EVERY == 0 or idx + 1 == total:
        print(f"[kat] {label} OK ({idx + 1}/{total})", flush=True)


def one_round(seed_d: int, run_mode: str, skip_build: bool, label: str, idx: int, total: int) -> None:
    d = derand_bytes_from_seed(seed_d)
    ek_oqs, dk_oqs = liboqs_pke_keygen(d)
    ek_dev, dk_dev = run_ascendc_keygen(seed_d, run_mode, skip_build)
    compare_pke(seed_d, label, ek_oqs, dk_oqs, ek_dev, dk_dev)
    _print_progress(label, idx, total)
    if run_mode == "sim" and not VERBOSE:
        print_sim_metrics_console(LOG_PATH)


def main() -> int:
    cpu_count = int(os.environ.get("KAT_CPU_COUNT", "10"))
    sim_count = int(os.environ.get("KAT_SIM_COUNT", "1"))
    if cpu_count < 1 and sim_count < 1:
        raise SystemExit("KAT_CPU_COUNT and KAT_SIM_COUNT cannot both be 0")

    _ensure_liboqs_ref()

    if not VERBOSE:
        LOG_PATH.parent.mkdir(parents=True, exist_ok=True)
        LOG_PATH.write_text(f"[kat] log started\n", encoding="utf-8")
        print(f"[kat] quiet — run.sh logs -> {LOG_PATH}", flush=True)

    print(f"[kat] start: CPU×{cpu_count} + SIM×{sim_count} (每轮独立随机 SEED_D)", flush=True)

    cpu_seeds = [random_seed_d() for _ in range(cpu_count)]
    for i, seed_d in enumerate(cpu_seeds):
        one_round(seed_d, "cpu", skip_build=(i > 0), label="CPU", idx=i, total=cpu_count)

    sim_seeds = [random_seed_d() for _ in range(sim_count)]
    for i, seed_d in enumerate(sim_seeds):
        one_round(seed_d, "sim", skip_build=False, label="SIM", idx=i, total=sim_count)

    print(
        f"[kat] PASS CPU×{cpu_count} + SIM×{sim_count} "
        f"(liboqs ml_kem_1024 PKE vs AscendC KeyGen, same SEED_D per round)",
        flush=True,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
