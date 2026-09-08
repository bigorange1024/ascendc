#!/usr/bin/python3
# coding=utf-8
# EN06：软对拍 Prep/NTT/Matvec/INTT + Pack(c)；Pack 失败仅 soft-fail 打印

import sys
import numpy as np

relative_tol = 0
absolute_tol = 0
error_tol = 0


def verify_one(label, output_path, golden_path, dtype=np.int32):
    """对拍单段；打印 error ratio；返回是否通过。"""
    output = np.fromfile(output_path, dtype=dtype).reshape(-1)
    golden = np.fromfile(golden_path, dtype=dtype).reshape(-1)
    if output.size != golden.size:
        print(f"[{label}] size mismatch: out={output.size} golden={golden.size}")
        return False
    close = np.isclose(output, golden, rtol=relative_tol, atol=absolute_tol, equal_nan=True)
    bad = np.where(close == False)[0]
    for index in range(min(len(bad), 32)):
        i = bad[index]
        print(
            f"[{label}] idx={i:06d} expected={golden[i]}({golden[i]:x}) "
            f"actual={output[i]}({output[i]:x}) diff={int(output[i]) - int(golden[i])}"
        )
    error_ratio = float(bad.size) / float(golden.size) if golden.size else 1.0
    print(f"[{label}] error ratio: {error_ratio:.4f}, tolerance: {error_tol:.4f}, bad={bad.size}")
    return error_ratio <= error_tol


def verify_pack_bytes(label, output_path, golden_path):
    """密文逐字节对拍（uint8）。"""
    output = np.fromfile(output_path, dtype=np.uint8).reshape(-1)
    golden = np.fromfile(golden_path, dtype=np.uint8).reshape(-1)
    if output.size != golden.size:
        print(f"[{label}] size mismatch: out={output.size} golden={golden.size}")
        return False
    bad = np.where(output != golden)[0]
    for index in range(min(len(bad), 32)):
        i = bad[index]
        print(
            f"[{label}] idx={i:06d} expected={golden[i]:02x} actual={output[i]:02x}"
        )
    error_ratio = float(bad.size) / float(golden.size) if golden.size else 1.0
    print(f"[{label}] error ratio: {error_ratio:.4f}, bad={bad.size}/{golden.size}")
    return bad.size == 0


if __name__ == "__main__":
    try:
        ok_prep = verify_one("Prep", "output/dst_prep.bin", "output/golden_prep.bin")
        ok_ntt = verify_one("NTT", "output/dst_ntt.bin", "output/golden_ntt.bin")
        ok_matvec = verify_one("Matvec", "output/dst_matvec.bin", "output/golden_matvec.bin")
        ok_intt = verify_one("INTT", "output/dst_intt.bin", "output/golden_intt.bin")
        ok_pack = verify_pack_bytes("Pack", "output/dst_pack.bin", "output/golden_pack.bin")
        if not (ok_prep and ok_ntt and ok_matvec and ok_intt):
            raise ValueError("[ERROR] Prep/NTT/Matvec/INTT golden mismatch")
        if not ok_pack:
            # Pack 对拍尽量做；失败不否决「不挂」（与任务书 soft-fail 一致）
            print("[WARN] Pack golden soft-fail (correctness secondary)")
            print("test pass (Prep+NTT+Matvec+INTT); Pack soft-fail")
        else:
            print("test pass (Prep+NTT+Matvec+INTT+Pack)")
    except Exception as e:
        print(e)
        sys.exit(1)
