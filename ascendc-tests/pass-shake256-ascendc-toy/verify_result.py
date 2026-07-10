#!/usr/bin/env python3
# coding=utf-8
"""对拍：设备 UB 自检 device_pass + Host golden（tiny_sha3 / Python）。

本文件在流水线中的位置：pass-shake256-ascendc-toy 探针的最终验收脚本，由 run.sh
在设备核跑完之后调用，与 pass-shake128-ops-math-toy/verify_result.py 结构完全对称
（仅哈希函数、C 库名不同）。三方对拍链路：
  1. 读取 main.cpp 落盘的 output/device_pass.bin —— 设备 UB 内 SHAKE256 与内嵌
     golden 的自检结果；
  2. 用第三方 tiny_sha3 重算一遍 batch，与 gen_data.py 产出的 golden_y.bin
     （Python hashlib.shake_256）比对；
  3. 用本脚本内嵌的 Python SHAKE256 实现再独立重算一遍，与同一份 golden_y.bin 比对。
三者一致方可判定 PASS，属于「I/O 与 golden 一致」验收。
"""
import hashlib
import struct
import subprocess
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent


def load_meta() -> tuple[int, int, int]:
    """读取 input/meta.bin，返回 (batch, max_msg_len, out_len)。"""
    buf = (ROOT / "input" / "meta.bin").read_bytes()
    return struct.unpack("<III", buf)


def python_shake_batch(x: bytes, lengths: list[int], max_msg_len: int, out_len: int) -> bytes:
    """用 Python 标准库 hashlib.shake_256 独立重算一遍 batch，作为第三方对拍来源之一。"""
    batch = len(lengths)
    golden = bytearray()
    for i in range(batch):
        # 按跨距切出第 i 条消息的真实部分
        msg = x[i * max_msg_len : i * max_msg_len + lengths[i]]
        golden.extend(hashlib.shake_256(msg).digest(out_len))
    return bytes(golden)


def check_bytes(label: str, got: np.ndarray, ref: np.ndarray) -> int:
    """逐元素比对两个 uint8 数组，形状不符或存在差异即 FAIL。返回 0=一致，1=不一致。"""
    if got.shape != ref.shape:
        print(f"[FAIL] {label} shape {got.shape} vs {ref.shape}")
        return 1
    diff = np.abs(got.astype(np.int16) - ref.astype(np.int16))
    mx = int(diff.max())
    if mx != 0:
        idx = int(diff.argmax())
        print(
            f"[FAIL] {label} max_abs_diff={mx} idx={idx} "
            f"got={int(got.reshape(-1)[idx])} ref={int(ref.reshape(-1)[idx])}"
        )
        return 1
    print(f"[SUCCESS] {label} max_abs_diff=0")
    return 0


def tiny_sha3_batch() -> np.ndarray:
    """编译（若需要）并调用 host_shake256_ref.c + thirdparty/tiny_sha3，重算一遍当前用例。

    编译产物缓存在 build_ref/libhost_shake256_ref.so，仅源文件更新时才重新编译。
    通过 ctypes 调用 C 函数 host_shake256_batch，参数语义见 host_shake256_ref.h。
    """
    import ctypes

    build_dir = ROOT / "build_ref"
    build_dir.mkdir(exist_ok=True)
    repo = ROOT.parent.parent
    sha3_root = repo / "thirdparty/tiny_sha3"
    so_path = build_dir / "libhost_shake256_ref.so"
    ref_c = ROOT / "host_shake256_ref.c"
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
    # 与 host_shake256_ref.h 中 host_shake256_batch 的形参顺序/类型严格对应
    lib.host_shake256_batch.argtypes = [
        ctypes.c_void_p,
        ctypes.c_void_p,
        ctypes.c_void_p,
        ctypes.c_uint32,
        ctypes.c_uint32,
        ctypes.c_uint32,
    ]
    out = np.zeros(y_size, dtype=np.uint8)
    lib.host_shake256_batch(
        out.ctypes.data_as(ctypes.c_void_p),
        ctypes.c_char_p(x),
        (ctypes.c_uint32 * batch)(*lengths),
        ctypes.c_uint32(batch),
        ctypes.c_uint32(max_msg_len),
        ctypes.c_uint32(out_len),
    )
    return out


def main() -> int:
    """验收主流程：设备自检 → tiny_sha3 对拍 → 内嵌 Python 对拍，三者均一致才算 PASS。"""
    rc = 0
    pass_path = ROOT / "output" / "device_pass.bin"
    if not pass_path.is_file():
        print("[FAIL] missing output/device_pass.bin")
        return 1
    # 第一路：设备核在 UB 内自检得到的结果
    device_pass = struct.unpack("<I", pass_path.read_bytes())[0]
    if device_pass != 1:
        print(f"[FAIL] device UB self-check device_pass={device_pass}")
        rc |= 1
    else:
        print("[SUCCESS] device UB self-check device_pass=1")

    batch, max_msg_len, out_len = load_meta()
    golden = np.fromfile(ROOT / "output" / "golden_y.bin", dtype=np.uint8)

    # 第二路：Host 侧 tiny_sha3（C 实现）独立重算，与 Python golden 比对
    tiny = tiny_sha3_batch()
    rc |= check_bytes("Host tiny_sha3 vs Python golden_y", tiny, golden)

    # 第三路：本脚本内嵌 Python hashlib 重算一遍，交叉验证 golden 自洽性
    x = (ROOT / "input" / "x.bin").read_bytes()
    lengths = list(struct.unpack(f"<{batch}I", (ROOT / "input" / "lengths.bin").read_bytes()))
    py = np.frombuffer(python_shake_batch(x, lengths, max_msg_len, out_len), dtype=np.uint8)
    rc |= check_bytes("Host inline Python vs golden_y", py, golden)

    if rc == 0:
        print("[verify] PASS")
    return rc


if __name__ == "__main__":
    sys.exit(main())
