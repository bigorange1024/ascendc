#!/usr/bin/env python3
"""
kat_liboqs_kem_encaps.py — liboqs KEM Encaps ↔ 探针旁路 A 批测（默认 quiet）。

固定 stash 公钥 ek_kem；每轮 os.urandom(32B) encaps_seed=m，
liboqs encaps_derand 与 device（KEM_ENC_EXT_SEED=1）对拍 c/K。

前置：bash scripts/kem_keypair_stash_bootstrap.sh

用法：
  bash scripts/liboqs_kem_encaps_batch.sh
  KEM_ENC_CPU_TRIALS=3 bash scripts/liboqs_kem_encaps_batch.sh
  KEM_ENC_VERBOSE=1 bash scripts/liboqs_kem_encaps_batch.sh
"""
from __future__ import annotations

import os
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np

REPO_ROOT = Path(__file__).resolve().parent.parent
ENCAPS_DIR = Path(os.environ.get("ENCAPS_DIR", REPO_ROOT / "ascendc-tests/fix-f203-alg20-kem-encaps-correctness-k4"))
STASH = Path(os.environ.get("KEM_KEYPAIR_STASH", REPO_ROOT / "output/kem_keypair_stash"))
REF_BIN = REPO_ROOT / "scripts/liboqs_kem_ref"
BUILD_REF = REPO_ROOT / "scripts/build_liboqs_kem_ref.sh"

ENCAPS_SEED_BYTES = 32
EK_BYTES = 1568
CT_BYTES = 1568
SS_BYTES = 32
SOC_VERSION = os.environ.get("SOC_VERSION", "Ascend910B4")
VERBOSE = os.environ.get("KEM_ENC_VERBOSE", os.environ.get("KAT_VERBOSE", "0")) == "1"
LOG_PATH = Path(os.environ.get("KEM_ENC_LOG", str(REPO_ROOT / "output/liboqs_kem_encaps/kat.log")))


def _fail(label: str, round_no: int, total: int, msg: str) -> None:
    raise SystemExit(f"[KAT FAIL] {label} round {round_no}/{total}: {msg}")


def _ensure_ref() -> Path:
    if REF_BIN.is_file():
        return REF_BIN
    kw = {} if VERBOSE else {"stdout": subprocess.DEVNULL, "stderr": subprocess.DEVNULL}
    subprocess.check_call(["bash", str(BUILD_REF)], **kw)
    return REF_BIN


def _load_stash_ek() -> np.ndarray:
    ek_path = STASH / "ek_kem.bin"
    if not ek_path.is_file():
        raise SystemExit(
            f"[kat] missing {ek_path} — run: bash scripts/kem_keypair_stash_bootstrap.sh"
        )
    ek = np.fromfile(ek_path, dtype=np.uint8)
    if ek.size != EK_BYTES:
        raise SystemExit(f"[kat] bad stash ek size={ek.size}")
    return ek


def _liboqs_encaps(ek: bytes, encaps_seed: bytes) -> tuple[np.ndarray, np.ndarray]:
    ref = _ensure_ref()
    with tempfile.TemporaryDirectory(prefix="oqs_enc_") as td:
        td_path = Path(td)
        ek_path = td_path / "ek.bin"
        c_path = td_path / "c.bin"
        k_path = td_path / "K.bin"
        ek_path.write_bytes(ek)
        subprocess.check_call(
            [str(ref), "encaps", str(ek_path), str(c_path), str(k_path), encaps_seed.hex()],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        c = np.fromfile(c_path, dtype=np.uint8)
        k = np.fromfile(k_path, dtype=np.uint8)
    if c.size != CT_BYTES or k.size != SS_BYTES:
        raise SystemExit(f"liboqs encaps size c={c.size} K={k.size}")
    return c, k


def _run_ascendc_encaps(ek: bytes, encaps_seed: bytes, run_mode: str) -> tuple[np.ndarray, np.ndarray]:
    inp = ENCAPS_DIR / "input"
    inp.mkdir(parents=True, exist_ok=True)
    (inp / "encaps_seed.bin").write_bytes(encaps_seed)

    env = os.environ.copy()
    env["KEM_ENC_EXT_SEED"] = "1"
    env["KEM_ENCAPS_VERIFY"] = "0"
    env["KEM_ENCAPS_KAT"] = "0" if VERBOSE else "1"
    env["EK_KEM_SRC"] = str(STASH / "ek_kem.bin")
    cmd = ["bash", "run.sh", "-r", run_mode, "-v", SOC_VERSION]

    if VERBOSE:
        rc = subprocess.run(cmd, cwd=ENCAPS_DIR, env=env).returncode
    else:
        LOG_PATH.parent.mkdir(parents=True, exist_ok=True)
        with LOG_PATH.open("a", encoding="utf-8") as logf:
            logf.write(f"\n{'=' * 72}\n[kat] encaps {run_mode} m={encaps_seed.hex()}\n")
            logf.flush()
        rc = subprocess.run(cmd, cwd=ENCAPS_DIR, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode

    if rc != 0:
        raise SystemExit(f"encaps run.sh({run_mode}) exit={rc} — see {LOG_PATH}")

    c = np.fromfile(ENCAPS_DIR / "output" / "c.bin", dtype=np.uint8)
    k = np.fromfile(ENCAPS_DIR / "output" / "K.bin", dtype=np.uint8)
    if c.size != CT_BYTES or k.size != SS_BYTES:
        raise SystemExit(f"device output size c={c.size} K={k.size}")
    return c, k


def _compare(round_no: int, total: int, label: str, c_oqs, k_oqs, c_dev, k_dev) -> None:
    if not np.array_equal(c_dev, c_oqs):
        idx = int(np.argmax(c_dev != c_oqs))
        _fail(label, round_no, total, f"c @ {idx}: device={c_dev.flat[idx]} liboqs={c_oqs.flat[idx]}")
    if not np.array_equal(k_dev, k_oqs):
        idx = int(np.argmax(k_dev != k_oqs))
        _fail(label, round_no, total, f"K @ {idx}: device={k_dev.flat[idx]} liboqs={k_oqs.flat[idx]}")


def _one_round(ek_bytes: bytes, encaps_seed: bytes, run_mode: str, label: str, round_no: int, total: int) -> None:
    c_oqs, k_oqs = _liboqs_encaps(ek_bytes, encaps_seed)
    c_dev, k_dev = _run_ascendc_encaps(ek_bytes, encaps_seed, run_mode)
    _compare(round_no, total, label, c_oqs, k_oqs, c_dev, k_dev)


def main() -> int:
    cpu_count = int(os.environ.get("KEM_ENC_CPU_TRIALS", "10"))
    sim_count = int(os.environ.get("KEM_ENC_SIM_TRIALS", "1"))
    only_mode = os.environ.get("KEM_ENC_ONLY_MODE", "").strip()
    if only_mode == "cpu":
        sim_count = 0
    elif only_mode == "sim":
        cpu_count = 0
    if cpu_count + sim_count == 0:
        raise SystemExit("KEM_ENC_CPU_TRIALS / KEM_ENC_SIM_TRIALS need at least one trial")

    ek_arr = _load_stash_ek()
    ek_bytes = ek_arr.tobytes()
    _ensure_ref()

    if not VERBOSE:
        LOG_PATH.parent.mkdir(parents=True, exist_ok=True)
        LOG_PATH.write_text("[kat] kem encaps vs liboqs (ext-seed, fixed ek)\n", encoding="utf-8")
        print(f"[kat] quiet — logs -> {LOG_PATH}")

    print(
        f"[kat] start CPU×{cpu_count} + SIM×{sim_count} "
        f"(fixed ek from {STASH}, os.urandom m ↔ KEM_ENC_EXT_SEED=1)"
    )

    os.environ.setdefault("KEM_ENCAPS_SKIP_REBUILD", "0")

    for i in range(cpu_count):
        m = os.urandom(ENCAPS_SEED_BYTES)
        _one_round(ek_bytes, m, "cpu", "CPU", i + 1, cpu_count)
        if i == 0:
            os.environ["KEM_ENCAPS_SKIP_REBUILD"] = "1"

    if cpu_count > 0:
        print(f"[kat] CPU {cpu_count}/{cpu_count} OK", flush=True)

    for i in range(sim_count):
        os.environ.pop("KEM_ENCAPS_SKIP_REBUILD", None)
        m = os.urandom(ENCAPS_SEED_BYTES)
        _one_round(ek_bytes, m, "sim", "SIM", i + 1, sim_count)

    if sim_count > 0:
        print(f"[kat] SIM {sim_count}/{sim_count} OK", flush=True)

    print(f"[kat] PASS CPU×{cpu_count} + SIM×{sim_count} (encaps c/K match liboqs, same m)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
