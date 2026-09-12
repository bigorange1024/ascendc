#!/usr/bin/python3
# coding=utf-8
"""
EN13：硬门禁 c vs liboqs max=0；中间段 soft 打印不否决。
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

import numpy as np

relative_tol = 0
absolute_tol = 0
error_tol = 0

_REPO = Path(__file__).resolve().parents[4]


def verify_one(label, output_path, golden_path, dtype=np.int32):
    """中间段 soft 对拍。"""
    if not Path(output_path).is_file() or not Path(golden_path).is_file():
        print(f"[{label}] SKIP missing {output_path} or {golden_path}")
        return False
    output = np.fromfile(output_path, dtype=dtype).reshape(-1)
    golden = np.fromfile(golden_path, dtype=dtype).reshape(-1)
    if output.size != golden.size:
        print(f"[{label}] size mismatch: out={output.size} golden={golden.size}")
        return False
    bad = np.where(output != golden)[0]
    for index in range(min(len(bad), 8)):
        i = bad[index]
        print(
            f"[{label}] idx={i:06d} expected={golden[i]} actual={output[i]} "
            f"diff={int(output[i]) - int(golden[i])}"
        )
    error_ratio = float(bad.size) / float(golden.size) if golden.size else 1.0
    print(f"[{label}] error ratio: {error_ratio:.4f}, bad={bad.size}/{golden.size}")
    return bad.size == 0


def verify_c_hard() -> bool:
    """硬门禁：output/c.bin ≡ liboqs fixture c（max=0）。"""
    asc_c = Path("output/c.bin")
    if not asc_c.is_file():
        # 兼容旧名
        asc_c = Path("output/dst_pack.bin")
    if not asc_c.is_file():
        print("[c] FAIL missing output/c.bin")
        return False
    # 复制到标准名供 liboqs_pke_vs_ascendc_verify
    if asc_c.name != "c.bin":
        Path("output/c.bin").write_bytes(asc_c.read_bytes())
    fixture = Path("input/liboqs_fixture")
    cmd = [
        sys.executable,
        str(_REPO / "scripts" / "liboqs_pke_vs_ascendc_verify.py"),
        "--stage",
        "encrypt",
        "--fixture",
        str(fixture),
        "--ascendc",
        "output",
    ]
    print("[c] running", " ".join(cmd))
    rc = subprocess.call(cmd)
    if rc != 0:
        # 额外打印 max
        got = np.fromfile("output/c.bin", dtype=np.uint8)
        ref = np.fromfile(fixture / "c.bin", dtype=np.uint8)
        diff = np.abs(got.astype(np.int16) - ref.astype(np.int16))
        mx = int(diff.max()) if diff.size else -1
        print(f"[c] HARD FAIL max={mx}")
        return False
    got = np.fromfile("output/c.bin", dtype=np.uint8)
    ref = np.fromfile(fixture / "c.bin", dtype=np.uint8)
    print(f"[c] HARD PASS max=0 ({got.size} bytes)")
    return True


if __name__ == "__main__":
    soft = []
    soft.append(("SampleNTT", verify_one("SampleNTT", "output/dst_a_hat.bin", "output/golden_a_hat.bin")))
    soft.append(("Prep", verify_one("Prep", "output/dst_prep.bin", "output/golden_prep.bin")))
    soft.append(("NTT", verify_one("NTT", "output/dst_ntt.bin", "output/golden_ntt.bin")))
    soft.append(("Matvec", verify_one("Matvec", "output/dst_matvec.bin", "output/golden_matvec.bin")))
    soft.append(("Dot", verify_one("Dot", "output/dst_dot.bin", "output/golden_dot.bin")))
    soft.append(("INTT_u", verify_one("INTT_u", "output/dst_intt_u.bin", "output/golden_intt_u.bin")))
    soft.append(("u_noisy", verify_one("u_noisy", "output/dst_u_noisy.bin", "output/golden_u_noisy.bin")))
    soft.append(("v_noisy", verify_one("v_noisy", "output/dst_v_noisy.bin", "output/golden_v_noisy.bin")))
    soft_ok = all(x[1] for x in soft)
    print(
        "[SOFT] "
        + " ".join(f"{n}={'ok' if v else 'FAIL'}" for n, v in soft)
        + ("" if soft_ok else " (soft only)")
    )

    hard_ok = verify_c_hard()
    if not hard_ok:
        print("test FAIL: c vs liboqs max≠0")
        sys.exit(1)
    print("test pass (EN13 Encrypt c ≡ liboqs)")
    sys.exit(0)
