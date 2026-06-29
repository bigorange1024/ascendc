#!/usr/bin/env python3
# coding=utf-8
"""对拍：设备 UB 自检 device_pass + Host golden（tiny_sha3 / Python）。"""
import hashlib
import struct
import subprocess
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent


def load_meta() -> tuple[int, int, int]:
    buf = (ROOT / "input" / "meta.bin").read_bytes()
    return struct.unpack("<III", buf)


def python_shake_batch(x: bytes, lengths: list[int], max_msg_len: int, out_len: int) -> bytes:
    batch = len(lengths)
    golden = bytearray()
    for i in range(batch):
        msg = x[i * max_msg_len : i * max_msg_len + lengths[i]]
        golden.extend(hashlib.shake_128(msg).digest(out_len))
    return bytes(golden)


def check_bytes(label: str, got: np.ndarray, ref: np.ndarray) -> int:
    if got.shape != ref.shape:
        print(f"[FAIL] {label} shape {got.shape} vs {ref.shape}")
        return 1
    diff = np.abs(got.astype(np.int16) - ref.astype(np.int16))
    mx = int(diff.max())
    if mx != 0:
        idx = int(diff.argmax())
        print(f"[FAIL] {label} max_abs_diff={mx} idx={idx} got={int(got.reshape(-1)[idx])} ref={int(ref.reshape(-1)[idx])}")
        return 1
    print(f"[SUCCESS] {label} max_abs_diff=0")
    return 0


def tiny_sha3_batch() -> np.ndarray:
    import ctypes

    build_dir = ROOT / "build_ref"
    build_dir.mkdir(exist_ok=True)
    repo = ROOT.parent.parent
    sha3_root = repo / "thirdparty/tiny_sha3"
    so_path = build_dir / "libhost_shake128_ref.so"
    ref_c = ROOT / "host_shake128_ref.c"
    newest = max(ref_c.stat().st_mtime, (sha3_root / "sha3.c").stat().st_mtime)
    if not so_path.is_file() or so_path.stat().st_mtime < newest:
        subprocess.run(
            [
                "gcc",
                "-shared",
                "-fPIC",
                "-O2",
                f"-I{sha3_root}",
                str(ref_c),
                str(sha3_root / "sha3.c"),
                "-o",
                str(so_path),
            ],
            check=True,
        )
    batch, max_msg_len, out_len = load_meta()
    x = (ROOT / "input" / "x.bin").read_bytes()
    lengths = list(struct.unpack(f"<{batch}I", (ROOT / "input" / "lengths.bin").read_bytes()))
    y_size = batch * out_len
    lib = ctypes.CDLL(str(so_path))
    lib.host_shake128_batch.argtypes = [
        ctypes.c_void_p,
        ctypes.c_void_p,
        ctypes.c_void_p,
        ctypes.c_uint32,
        ctypes.c_uint32,
        ctypes.c_uint32,
    ]
    out = np.zeros(y_size, dtype=np.uint8)
    lib.host_shake128_batch(
        out.ctypes.data_as(ctypes.c_void_p),
        ctypes.c_char_p(x),
        (ctypes.c_uint32 * batch)(*lengths),
        ctypes.c_uint32(batch),
        ctypes.c_uint32(max_msg_len),
        ctypes.c_uint32(out_len),
    )
    return out


def main() -> int:
    rc = 0
    pass_path = ROOT / "output" / "device_pass.bin"
    if not pass_path.is_file():
        print("[FAIL] missing output/device_pass.bin")
        return 1
    device_pass = struct.unpack("<I", pass_path.read_bytes())[0]
    if device_pass != 1:
        print(f"[FAIL] device UB self-check device_pass={device_pass}")
        rc |= 1
    else:
        print("[SUCCESS] device UB self-check device_pass=1")

    batch, max_msg_len, out_len = load_meta()
    golden = np.fromfile(ROOT / "output" / "golden_y.bin", dtype=np.uint8)

    tiny = tiny_sha3_batch()
    rc |= check_bytes("Host tiny_sha3 vs Python golden_y", tiny, golden)

    x = (ROOT / "input" / "x.bin").read_bytes()
    lengths = list(struct.unpack(f"<{batch}I", (ROOT / "input" / "lengths.bin").read_bytes()))
    py = np.frombuffer(python_shake_batch(x, lengths, max_msg_len, out_len), dtype=np.uint8)
    rc |= check_bytes("Host inline Python vs golden_y", py, golden)

    if rc == 0:
        print("[verify] PASS")
    return rc


if __name__ == "__main__":
    sys.exit(main())
