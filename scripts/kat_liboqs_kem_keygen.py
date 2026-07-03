#!/usr/bin/env python3
"""
kat_liboqs_kem_keygen.py — liboqs KEM KeyGen ↔ 探针旁路 A 批测（默认 quiet）。

与 PKE `kat_liboqs_vs_ascendc.py` 同型：每轮 os.urandom 64B kem_seed = d‖z，
liboqs keypair_derand 与探针（KEM_KG_EXT_SEED=1）吃相同随机字节，对拍 ek_kem/dk_kem。

用法：
  bash scripts/liboqs_kem_keygen_batch.sh
  KEM_KG_CPU_TRIALS=3 KEM_KG_SIM_TRIALS=0 bash scripts/liboqs_kem_keygen_batch.sh
  KEM_KG_VERBOSE=1 bash scripts/liboqs_kem_keygen_batch.sh   # 全量 log（等同旧 batch）
"""
from __future__ import annotations

import os
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np

REPO_ROOT = Path(__file__).resolve().parent.parent
KEYGEN_DIR = Path(os.environ.get("KEYGEN_DIR", REPO_ROOT / "ascendc-tests/fix-f203-alg19-kem-keygen-k4"))
REF_BIN = REPO_ROOT / "scripts/liboqs_kem_ref"
BUILD_REF = REPO_ROOT / "scripts/build_liboqs_kem_ref.sh"

KEM_SEED_BYTES = 64
EK_BYTES = 1568
DK_BYTES = 3168
SOC_VERSION = os.environ.get("SOC_VERSION", "Ascend910B4")
VERBOSE = os.environ.get("KEM_KG_VERBOSE", os.environ.get("KAT_VERBOSE", "0")) == "1"
LOG_PATH = Path(os.environ.get("KEM_KG_LOG", str(REPO_ROOT / "output/liboqs_kem_keygen/kat.log")))
FX_ROOT = Path(os.environ.get("LIBOQS_KEM_KEYGEN_FX_DIR", str(REPO_ROOT / "output/liboqs_kem_keygen")))


def _fail(label: str, round_no: int, total: int, kem_seed_hex: str, msg: str) -> None:
    raise SystemExit(f"[KAT FAIL] {label} round {round_no}/{total} kem_seed={kem_seed_hex[:16]}…: {msg}")


def _ensure_ref() -> Path:
    if REF_BIN.is_file():
        return REF_BIN
    kw = {} if VERBOSE else {"stdout": subprocess.DEVNULL, "stderr": subprocess.DEVNULL}
    subprocess.check_call(["bash", str(BUILD_REF)], **kw)
    return REF_BIN


def _liboqs_keygen(kem_seed: bytes) -> tuple[np.ndarray, np.ndarray]:
    ref = _ensure_ref()
    with tempfile.TemporaryDirectory(prefix="oqs_kem_kg_") as td:
        td_path = Path(td)
        ek_path = td_path / "ek_kem.bin"
        dk_path = td_path / "dk_kem.bin"
        subprocess.check_call(
            [str(ref), "keygen", str(ek_path), str(dk_path), kem_seed.hex()],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        ek = np.fromfile(ek_path, dtype=np.uint8)
        dk = np.fromfile(dk_path, dtype=np.uint8)
    if ek.size != EK_BYTES or dk.size != DK_BYTES:
        raise SystemExit(f"liboqs size ek={ek.size} dk={dk.size}")
    return ek, dk


def _run_ascendc_keygen(kem_seed: bytes, run_mode: str) -> tuple[np.ndarray, np.ndarray]:
    inp = KEYGEN_DIR / "input"
    inp.mkdir(parents=True, exist_ok=True)
    (inp / "kem_seed.bin").write_bytes(kem_seed)

    env = os.environ.copy()
    env["KEM_KG_EXT_SEED"] = "1"
    env["KEM_KEYGEN_VERIFY"] = "0"
    env["KEM_KEYGEN_KAT"] = "0" if VERBOSE else "1"
    cmd = ["bash", "run.sh", "-r", run_mode, "-v", SOC_VERSION]

    if VERBOSE:
        rc = subprocess.run(cmd, cwd=KEYGEN_DIR, env=env).returncode
    else:
        LOG_PATH.parent.mkdir(parents=True, exist_ok=True)
        with LOG_PATH.open("a", encoding="utf-8") as logf:
            logf.write(f"\n{'=' * 72}\n[kat] kem_keygen {run_mode} kem_seed={kem_seed.hex()}\n")
            logf.flush()
        rc = subprocess.run(cmd, cwd=KEYGEN_DIR, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode

    if rc != 0:
        raise SystemExit(f"run.sh({run_mode}) exit={rc} — see {LOG_PATH}")

    ek = np.fromfile(KEYGEN_DIR / "output" / "ek_kem.bin", dtype=np.uint8)
    dk = np.fromfile(KEYGEN_DIR / "output" / "dk_kem.bin", dtype=np.uint8)
    if ek.size != EK_BYTES or dk.size != DK_BYTES:
        raise SystemExit(f"device output size ek={ek.size} dk={dk.size}")
    return ek, dk


def _compare(round_no: int, total: int, label: str, kem_seed: bytes, ek_oqs, dk_oqs, ek_dev, dk_dev) -> None:
    hx = kem_seed.hex()
    if not np.array_equal(ek_dev, ek_oqs):
        idx = int(np.argmax(ek_dev != ek_oqs))
        _fail(label, round_no, total, hx, f"ek_kem @ {idx}: device={ek_dev.flat[idx]} liboqs={ek_oqs.flat[idx]}")
    if not np.array_equal(dk_dev, dk_oqs):
        idx = int(np.argmax(dk_dev != dk_oqs))
        _fail(label, round_no, total, hx, f"dk_kem @ {idx}: device={dk_dev.flat[idx]} liboqs={dk_oqs.flat[idx]}")


def _one_round(kem_seed: bytes, run_mode: str, label: str, round_no: int, total: int) -> None:
    # 可选：落盘 fixture 供失败排查（quiet 时不打印）
    if not VERBOSE:
        fx = FX_ROOT / run_mode / str(round_no)
        fx.mkdir(parents=True, exist_ok=True)
        (fx / "kem_seed.bin").write_bytes(kem_seed)

    ek_oqs, dk_oqs = _liboqs_keygen(kem_seed)
    ek_dev, dk_dev = _run_ascendc_keygen(kem_seed, run_mode)
    _compare(round_no, total, label, kem_seed, ek_oqs, dk_oqs, ek_dev, dk_dev)

    if not VERBOSE:
        fx = FX_ROOT / run_mode / str(round_no)
        fx.mkdir(parents=True, exist_ok=True)
        ek_oqs.tofile(fx / "ek_kem.bin")
        dk_oqs.tofile(fx / "dk_kem.bin")

    # 首轮 CPU 通过后写入固定 keypair stash，供 encaps/decaps 分项 kat 复用
    if run_mode == "cpu" and round_no == 1:
        stash = Path(os.environ.get("KEM_KEYPAIR_STASH", str(REPO_ROOT / "output/kem_keypair_stash")))
        stash.mkdir(parents=True, exist_ok=True)
        ek_dev.tofile(stash / "ek_kem.bin")
        dk_dev.tofile(stash / "dk_kem.bin")
        (stash / "kem_seed.bin").write_bytes(kem_seed)
        if not VERBOSE:
            print(f"[kat] keypair stash -> {stash}/ (ek+dk from device)", flush=True)


def main() -> int:
    cpu_count = int(os.environ.get("KEM_KG_CPU_TRIALS", "10"))
    sim_count = int(os.environ.get("KEM_KG_SIM_TRIALS", "1"))
    only_mode = os.environ.get("KEM_KG_ONLY_MODE", "").strip()
    if only_mode == "cpu":
        sim_count = 0
    elif only_mode == "sim":
        cpu_count = 0

    if cpu_count < 0 or sim_count < 0 or (cpu_count + sim_count) == 0:
        raise SystemExit("KEM_KG_CPU_TRIALS / KEM_KG_SIM_TRIALS must be >= 0 with at least one trial")

    _ensure_ref()

    if not VERBOSE:
        LOG_PATH.parent.mkdir(parents=True, exist_ok=True)
        LOG_PATH.write_text("[kat] kem keygen vs liboqs (ext-seed)\n", encoding="utf-8")
        print(f"[kat] quiet — logs -> {LOG_PATH}")

    print(
        f"[kat] start CPU×{cpu_count} + SIM×{sim_count} "
        "(os.urandom kem_seed=d‖z ↔ liboqs keypair_derand vs KEM_KG_EXT_SEED=1)"
    )

    os.environ.setdefault("KEM_KEYGEN_SKIP_REBUILD", "0")

    for i in range(cpu_count):
        kem_seed = os.urandom(KEM_SEED_BYTES)
        _one_round(kem_seed, "cpu", "CPU", i + 1, cpu_count)
        if i == 0:
            os.environ["KEM_KEYGEN_SKIP_REBUILD"] = "1"

    if cpu_count > 0:
        print(f"[kat] CPU {cpu_count}/{cpu_count} OK", flush=True)

    for i in range(sim_count):
        os.environ.pop("KEM_KEYGEN_SKIP_REBUILD", None)
        kem_seed = os.urandom(KEM_SEED_BYTES)
        _one_round(kem_seed, "sim", "SIM", i + 1, sim_count)

    if sim_count > 0:
        print(f"[kat] SIM {sim_count}/{sim_count} OK", flush=True)

    print(f"[kat] PASS CPU×{cpu_count} + SIM×{sim_count} (kem keygen vs liboqs, same random kem_seed)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
