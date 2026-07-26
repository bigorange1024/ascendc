#!/usr/bin/env python3
"""
kat_liboqs_kem_decaps.py — liboqs 造密文 + device Decaps 分项批测（默认 quiet）。

固定 stash (ek, dk)；每轮 os.urandom(32B) m → liboqs encaps_derand(ek,m) 得 c/K_ref，
device Decaps(dk, c) 得 K_dev，对拍 K。

不跑 device Encaps，省时；密文每轮由 liboqs 生成（不可固定）。

前置：bash scripts/kem_keypair_stash_bootstrap.sh

用法：
  bash scripts/liboqs_kem_decaps_batch.sh
  KEM_DEC_CPU_TRIALS=3 bash scripts/liboqs_kem_decaps_batch.sh
"""
from __future__ import annotations

import os
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np

REPO_ROOT = Path(__file__).resolve().parent.parent
DECAPS_DIR = Path(
    os.environ.get(
        "DECAPS_DIR",
        REPO_ROOT / "examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-decaps-k4",
    )
)
STASH = Path(os.environ.get("KEM_KEYPAIR_STASH", REPO_ROOT / "output/kem_keypair_stash"))
REF_BIN = REPO_ROOT / "scripts/liboqs_kem_ref"
BUILD_REF = REPO_ROOT / "scripts/build_liboqs_kem_ref.sh"

ENCAPS_SEED_BYTES = 32
EK_BYTES = 1568
DK_BYTES = 3168
CT_BYTES = 1568
SS_BYTES = 32
SOC_VERSION = os.environ.get("SOC_VERSION", "Ascend910B4")
VERBOSE = os.environ.get("KEM_DEC_VERBOSE", os.environ.get("KAT_VERBOSE", "0")) == "1"
LOG_PATH = Path(os.environ.get("KEM_DEC_LOG", str(REPO_ROOT / "output/liboqs_kem_decaps/kat.log")))


def _fail(label: str, round_no: int, total: int, msg: str) -> None:
    raise SystemExit(f"[KAT FAIL] {label} round {round_no}/{total}: {msg}")


def _ensure_ref() -> Path:
    if REF_BIN.is_file():
        return REF_BIN
    kw = {} if VERBOSE else {"stdout": subprocess.DEVNULL, "stderr": subprocess.DEVNULL}
    subprocess.check_call(["bash", str(BUILD_REF)], **kw)
    return REF_BIN


def _load_stash() -> tuple[bytes, bytes]:
    ek_path = STASH / "ek_kem.bin"
    dk_path = STASH / "dk_kem.bin"
    if not ek_path.is_file() or not dk_path.is_file():
        raise SystemExit(
            f"[kat] missing stash under {STASH} — run: bash scripts/kem_keypair_stash_bootstrap.sh"
        )
    ek = ek_path.read_bytes()
    dk = dk_path.read_bytes()
    if len(ek) != EK_BYTES or len(dk) != DK_BYTES:
        raise SystemExit(f"[kat] bad stash sizes ek={len(ek)} dk={len(dk)}")
    return ek, dk


def _liboqs_encaps_derand(ek: bytes, m: bytes) -> tuple[bytes, bytes]:
    """liboqs 造 (c, K) 供 decaps 合法路径对拍。"""
    ref = _ensure_ref()
    with tempfile.TemporaryDirectory(prefix="oqs_dec_ct_") as td:
        td_path = Path(td)
        ek_p = td_path / "ek.bin"
        c_p = td_path / "c.bin"
        k_p = td_path / "K.bin"
        ek_p.write_bytes(ek)
        subprocess.check_call(
            [str(ref), "encaps", str(ek_p), str(c_p), str(k_p), m.hex()],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        c = c_p.read_bytes()
        k = k_p.read_bytes()
    if len(c) != CT_BYTES or len(k) != SS_BYTES:
        raise SystemExit(f"liboqs encaps size c={len(c)} K={len(k)}")
    return c, k


def _run_ascendc_decaps(dk: bytes, c: bytes, m: bytes, k_ref: bytes, run_mode: str) -> np.ndarray:
    fx_dir = DECAPS_DIR / "input"
    fx_dir.mkdir(parents=True, exist_ok=True)

    env = os.environ.copy()
    env["KEM_DECAPS_VERIFY"] = "0"
    env["KEM_DECAPS_TAMPER_C"] = "0"
    env["KEM_DECAPS_REJECT"] = "0"
    env["KEM_DECAPS_KAT"] = "0" if VERBOSE else "1"
    env["DK_KEM_SRC"] = str(STASH / "dk_kem.bin")
    # CPU twin Phase-E 仍读 input/golden_v.bin（由 gen_data 按 m 生成）；
    # 必须传入与 liboqs encaps 相同的 m，否则 c' 错 → FO 走拒绝支路。
    with tempfile.NamedTemporaryFile(prefix="kat_c_", suffix=".bin", delete=False) as tf:
        c_tmp = Path(tf.name)
    with tempfile.NamedTemporaryFile(prefix="kat_m_", suffix=".bin", delete=False) as tf:
        m_tmp = Path(tf.name)
    with tempfile.NamedTemporaryFile(prefix="kat_k_", suffix=".bin", delete=False) as tf:
        k_tmp = Path(tf.name)
    c_tmp.write_bytes(c)
    m_tmp.write_bytes(m)
    k_tmp.write_bytes(k_ref)
    env["C_SRC"] = str(c_tmp)
    env["M_FILE"] = str(m_tmp)
    env["K_ENC_SRC"] = str(k_tmp)
    cmd = ["bash", "run.sh", "-r", run_mode, "-v", SOC_VERSION]
    try:
        if VERBOSE:
            rc = subprocess.run(cmd, cwd=DECAPS_DIR, env=env).returncode
        else:
            LOG_PATH.parent.mkdir(parents=True, exist_ok=True)
            with LOG_PATH.open("a", encoding="utf-8") as logf:
                logf.write(f"\n{'=' * 72}\n[kat] decaps {run_mode} c_prefix={c[:8].hex()}…\n")
                logf.flush()
            rc = subprocess.run(
                cmd, cwd=DECAPS_DIR, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
            ).returncode
    finally:
        c_tmp.unlink(missing_ok=True)
        m_tmp.unlink(missing_ok=True)
        k_tmp.unlink(missing_ok=True)

    if rc != 0:
        raise SystemExit(f"decaps run.sh({run_mode}) exit={rc} — see {LOG_PATH}")

    k = np.fromfile(DECAPS_DIR / "output" / "K.bin", dtype=np.uint8)
    if k.size != SS_BYTES:
        raise SystemExit(f"device K size={k.size}")
    return k


def _one_round(ek: bytes, dk: bytes, m: bytes, run_mode: str, label: str, round_no: int, total: int) -> None:
    c, k_ref = _liboqs_encaps_derand(ek, m)
    k_dev = _run_ascendc_decaps(dk, c, m, k_ref, run_mode)
    k_ref_arr = np.frombuffer(k_ref, dtype=np.uint8)
    if not np.array_equal(k_dev, k_ref_arr):
        idx = int(np.argmax(k_dev != k_ref_arr))
        _fail(label, round_no, total, f"K @ {idx}: device={k_dev.flat[idx]} liboqs={k_ref_arr.flat[idx]}")


def main() -> int:
    cpu_count = int(os.environ.get("KEM_DEC_CPU_TRIALS", "10"))
    sim_count = int(os.environ.get("KEM_DEC_SIM_TRIALS", "3"))
    only_mode = os.environ.get("KEM_DEC_ONLY_MODE", "").strip()
    if only_mode == "cpu":
        sim_count = 0
    elif only_mode == "sim":
        cpu_count = 0
    if cpu_count + sim_count == 0:
        raise SystemExit("KEM_DEC_CPU_TRIALS / KEM_DEC_SIM_TRIALS need at least one trial")

    ek, dk = _load_stash()
    _ensure_ref()

    if not VERBOSE:
        LOG_PATH.parent.mkdir(parents=True, exist_ok=True)
        LOG_PATH.write_text("[kat] kem decaps vs liboqs (liboqs-generated c, fixed dk)\n", encoding="utf-8")
        print(f"[kat] quiet — logs -> {LOG_PATH}")

    print(
        f"[kat] start CPU×{cpu_count} + SIM×{sim_count} "
        f"(fixed dk from {STASH}, liboqs encaps→c per round, device Decaps)"
    )

    os.environ.setdefault("KEM_DECAPS_SKIP_REBUILD", "0")

    for i in range(cpu_count):
        m = os.urandom(ENCAPS_SEED_BYTES)
        _one_round(ek, dk, m, "cpu", "CPU", i + 1, cpu_count)
        if i == 0:
            os.environ["KEM_DECAPS_SKIP_REBUILD"] = "1"

    if cpu_count > 0:
        print(f"[kat] CPU {cpu_count}/{cpu_count} OK", flush=True)

    for i in range(sim_count):
        os.environ.pop("KEM_DECAPS_SKIP_REBUILD", None)
        m = os.urandom(ENCAPS_SEED_BYTES)
        _one_round(ek, dk, m, "sim", "SIM", i + 1, sim_count)

    if sim_count > 0:
        print(f"[kat] SIM {sim_count}/{sim_count} OK", flush=True)

    print(f"[kat] PASS CPU×{cpu_count} + SIM×{sim_count} (Decaps K match liboqs, liboqs-generated c)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
