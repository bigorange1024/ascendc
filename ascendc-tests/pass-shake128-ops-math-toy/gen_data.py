#!/usr/bin/env python3
# coding=utf-8
"""固定用例 golden：Python hashlib.shake_128 + ML-KEM PRF 形参 (σ||N, 128B)。

本文件在流水线中的位置：pass-shake128-ops-math-toy 探针的 Host 侧数据生成脚本，
由 run.sh 在设备编译/运行前调用。职责：
  1. 构造若干固定用例（abc / empty / prf_sigma_n0 / batch_mixed），
     用 Python 标准库 hashlib.shake_128 计算每个用例的 golden 输出；
  2. 把 `SHAKE128_CASE` 环境变量指定的用例落盘到 input/（meta.bin/x.bin/lengths.bin）
     与 output/golden_y.bin，供 main.cpp（Host）与 verify_result.py（对拍）使用；
  3. 额外把该用例的数据（消息、长度、golden 输出）固化为设备侧编译期常量
     auto_gen/toy_active_case.h（通过 scripts/emit_toy_active_case_h.py），
     因为本探针核函数不做 GM 上的 x/y 搬运，而是把用例数据直接编译进设备程序，
     在 UB 内自检（见 shake128_toy_ub.hpp）。
与 golden 的关系：本脚本产出的 golden_y.bin 是黑盒 oracle（仅验证 I/O 一致），
不是设备实现必须复刻的算法规格；prf_sigma_n0 用例的 σ 派生方式模拟 ML-KEM PRF
调用形参（33B = σ(32B)||N(1B)），但 SHAKE128 本身走的是通用 SHAKE128（非 FIPS 203
的 PRF 专用轨——PRF 专用轨为 SHAKE256，见 pass-shake256-ascendc-toy），此处仅作为
形参尺寸/内容的真实场景 shim 用例。
"""
from __future__ import annotations

import hashlib
import json
import os
import struct
import sys
from pathlib import Path

import numpy as np

SEED_D_DEFAULT = 20260619  # 与 exp-mlkem-f203-2s1e-k4 系列探针共用的默认种子，便于跨探针复现同一 σ
K = 4  # ML-KEM-1024 的模块秩 k，仅用于派生 sigma 时占位（与本探针核心逻辑无关）


def derand_and_sigma(seed_d: int) -> bytes:
    """由固定 seed_d 派生 ML-KEM 场景下的 σ（32B），用于构造 prf_sigma_n0 用例的真实感输入。

    派生方式：sha3_256(消息标签+seed_d) 得 d，再 sha3_512(d || K) 取 [32:64) 字节段作为 σ。
    该派生规则只是本探针为了拿到「像真的」33B PRF 形参而设计的固定构造，与 FIPS 203
    正式的 KeyGen 派生算法无关，不作为密码学规格使用。
    """
    msg = f"exp-mlkem-f203-2s1e-k4:SEED_D={seed_d}".encode()
    d = hashlib.sha3_256(msg).digest()
    buf = hashlib.sha3_512(d + bytes([K & 0xFF])).digest()
    return buf[32:64]


def write_case(name: str, messages: list[bytes], out_len: int, root: Path) -> dict:
    """构造并落盘一个 SHAKE128 用例（batch 语义）。

    Args:
        name: 用例名，对应 cases/<name>/ 子目录
        messages: 本用例内的消息列表（每条可不同长度），batch = len(messages)
        out_len: 每条消息的期望 SHAKE128 输出字节数（本用例内所有消息统一）
        root: 落盘根目录（本脚本调用处传入 out/"cases"）

    Returns:
        用例元信息 dict（name/batch/maxMsgLen/outLen/lengths），供 manifest.json 汇总记录。

    产出文件（cases/<name>/ 下）：
        x.bin        —— 行优先 [batch, maxMsgLen] uint8，消息按最大长度补零对齐存放
        lengths.bin  —— batch 个小端 uint32_t，每条消息真实字节长度
        golden_y.bin —— 行优先 [batch, outLen] uint8，Python hashlib.shake_128 计算的期望输出
        meta.bin     —— 3 个小端 uint32_t：batch/maxMsgLen/outLen
    """
    batch = len(messages)
    max_msg_len = max((len(m) for m in messages), default=0)
    if max_msg_len == 0:
        # 全部消息为空（如 empty 用例）时仍需 >=1 字节的占位跨距，避免 0 长度缓冲区
        max_msg_len = 1

    x = bytearray(batch * max_msg_len)
    lengths = []
    golden = bytearray()
    for i, msg in enumerate(messages):
        # 第 i 条消息写入 x 的 [i*max_msg_len, i*max_msg_len+len(msg)) 区间，尾部补零跨距不参与哈希
        x[i * max_msg_len : i * max_msg_len + len(msg)] = msg
        lengths.append(len(msg))
        # golden：Python 标准库 shake_128，按 out_len 截取可扩展输出
        golden.extend(hashlib.shake_128(msg).digest(out_len))

    case_dir = root / name
    case_dir.mkdir(parents=True, exist_ok=True)
    (case_dir / "x.bin").write_bytes(bytes(x))
    with open(case_dir / "lengths.bin", "wb") as f:
        for v in lengths:
            f.write(struct.pack("<I", v))
    (case_dir / "golden_y.bin").write_bytes(bytes(golden))

    meta = struct.pack("<III", batch, max_msg_len, out_len)
    (case_dir / "meta.bin").write_bytes(meta)

    return {
        "name": name,
        "batch": batch,
        "maxMsgLen": max_msg_len,
        "outLen": out_len,
        "lengths": lengths,
    }


def main() -> None:
    """脚本入口：生成全部固定用例 → 选中 SHAKE128_CASE 指定的一个 → 落盘 input/output → 生成设备头文件。"""
    seed_d = int(os.environ.get("SEED_D", str(SEED_D_DEFAULT)))
    sigma = derand_and_sigma(seed_d)
    prf_msg = sigma + bytes([0])  # ML-KEM PRF 形参 shim：σ(32B) || N(1B)，N=0 对应本用例固定取值

    out = Path(__file__).resolve().parent
    # 依次构造四个固定用例：abc（教科书三字节消息）、empty（空消息边界）、
    # prf_sigma_n0（33B→128B，模拟 ML-KEM PRF 调用形参规模，非规范 PRF 轨）、
    # batch_mixed（同一 batch 内混合空/短/跨块长度，覆盖 SHAKE128 分块吸收边界）
    cases = []
    cases.append(write_case("abc", [b"abc"], 32, out / "cases"))
    cases.append(write_case("empty", [b""], 32, out / "cases"))
    cases.append(write_case("prf_sigma_n0", [prf_msg], 128, out / "cases"))
    cases.append(
        write_case(
            "batch_mixed",
            [b"", b"abc", bytes(range(64)), bytes(range(137))],
            64,
            out / "cases",
        )
    )

    # 从环境变量选择本次实际跑的用例（默认 abc），未知用例名直接报错阻断
    active = os.environ.get("SHAKE128_CASE", "abc")
    active_dir = out / "cases" / active
    if not active_dir.is_dir():
        raise SystemExit(f"unknown SHAKE128_CASE={active}")

    input_dir = out / "input"
    output_dir = out / "output"
    input_dir.mkdir(exist_ok=True)
    output_dir.mkdir(exist_ok=True)

    # 把选中用例的三个输入文件复制到 input/（main.cpp 读取），golden 复制到 output/（verify_result.py 读取）
    for fn in ("meta.bin", "x.bin", "lengths.bin"):
        (input_dir / fn).write_bytes((active_dir / fn).read_bytes())
    (output_dir / "golden_y.bin").write_bytes((active_dir / "golden_y.bin").read_bytes())

    manifest = {"active": active, "cases": cases}
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")

    # 设备核不搬运 GM 上的 x/y，而是把当前用例数据编译进设备程序：
    # 调用 scripts/emit_toy_active_case_h.py 把 x/lengths/golden_y 转成 C++ 常量数组头文件
    sys.path.insert(0, str(out / "scripts"))
    from emit_toy_active_case_h import emit_toy_active_case_h  # noqa: E402

    emit_toy_active_case_h(active_dir, out / "auto_gen" / "toy_active_case.h", ns="Shake128ToyActive")
    print(f"[gen_data] SHAKE128_CASE={active} batch={manifest['cases']}")
    print(f"[gen_data] wrote auto_gen/toy_active_case.h")
    print(f"[gen_data] wrote input/ + output/golden_y.bin (Python shake_128)")


if __name__ == "__main__":
    main()
