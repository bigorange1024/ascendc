#!/usr/bin/env python3
# coding=utf-8
"""SHAKE256 golden：Python hashlib.shake_256 + FIPS PRF 形参 (σ||N, 128B)。

本文件在流水线中的位置：pass-shake256-ascendc-toy 探针的 Host 侧数据生成脚本，
由 run.sh 在设备编译/运行前调用，与 pass-shake128-ops-math-toy/gen_data.py
结构完全对称，区别在于：
  1. 用 hashlib.shake_256 而非 shake_128 计算 golden；
  2. 额外构造 rate_135/rate_136/rate_137 三个用例，专门覆盖 SHAKE256 吸收速率
     （rate=136B）边界附近的消息长度（恰好小于/等于/超过一个 block），验证
     设备端分块吸收逻辑在跨块边界时的正确性；
  3. SHAKE256 是 FIPS 203 ML-KEM 中 PRF/H/G/J 等函数的规范轨基础原语（区别于
     SHAKE128 shim 轨），prf_sigma_n0 用例同样模拟 33B→128B 的 PRF 调用形参规模。
本脚本产出的 golden_y.bin 仅作黑盒 oracle（验证 I/O 一致），不是设备实现须复刻
的算法规格。
"""
from __future__ import annotations

import hashlib
import json
import os
import struct
import sys
from pathlib import Path

SEED_D_DEFAULT = 20260619  # 与 exp-mlkem-f203-2s1e-k4 系列探针共用的默认种子
K = 4  # ML-KEM-1024 模块秩 k，仅用于派生 sigma 时占位


def derand_and_sigma(seed_d: int) -> bytes:
    """由固定 seed_d 派生 ML-KEM 场景下的 σ（32B），用于构造 prf_sigma_n0 用例。

    派生方式：sha3_256(消息标签+seed_d) 得 d，再 sha3_512(d || K) 取 [32:64) 字节段。
    仅为构造「像真的」PRF 形参，不是 FIPS 203 KeyGen 正式派生算法。
    """
    msg = f"exp-mlkem-f203-2s1e-k4:SEED_D={seed_d}".encode()
    d = hashlib.sha3_256(msg).digest()
    buf = hashlib.sha3_512(d + bytes([K & 0xFF])).digest()
    return buf[32:64]


def write_case(name: str, messages: list[bytes], out_len: int, root: Path) -> dict:
    """构造并落盘一个 SHAKE256 用例（batch 语义），产出文件同 SHAKE128 版 write_case。

    Args:
        name: 用例名，对应 cases/<name>/ 子目录
        messages: 本用例内的消息列表
        out_len: 每条消息的期望 SHAKE256 输出字节数
        root: 落盘根目录
    Returns:
        用例元信息 dict，供 manifest.json 汇总记录。
    """
    batch = len(messages)
    max_msg_len = max((len(m) for m in messages), default=0)
    if max_msg_len == 0:
        # 全部消息为空时仍需 >=1 字节占位跨距
        max_msg_len = 1

    x = bytearray(batch * max_msg_len)
    lengths = []
    golden = bytearray()
    for i, msg in enumerate(messages):
        # 第 i 条消息写入 x 的对应跨距区间，尾部补零不参与哈希
        x[i * max_msg_len : i * max_msg_len + len(msg)] = msg
        lengths.append(len(msg))
        golden.extend(hashlib.shake_256(msg).digest(out_len))

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
    """脚本入口：生成全部固定用例 → 选中 SHAKE256_CASE 指定的一个 → 落盘 input/output → 生成设备头文件。"""
    seed_d = int(os.environ.get("SEED_D", str(SEED_D_DEFAULT)))
    sigma = derand_and_sigma(seed_d)
    prf_msg = sigma + bytes([0])  # ML-KEM PRF 形参 shim：σ(32B) || N(1B)，N=0

    out = Path(__file__).resolve().parent
    # 固定用例集合：abc/empty 基础用例；prf_sigma_n0 模拟真实 PRF 调用规模；
    # rate_135/136/137 专门覆盖 SHAKE256 吸收速率（136B）边界前后一个字节，
    # 验证设备端跨块吸收逻辑在恰好不足/刚好等于/刚好超过一个 block 时均正确；
    # batch_mixed 同一 batch 内混合空/短/跨块长度
    cases = []
    cases.append(write_case("abc", [b"abc"], 32, out / "cases"))
    cases.append(write_case("empty", [b""], 32, out / "cases"))
    cases.append(write_case("prf_sigma_n0", [prf_msg], 128, out / "cases"))
    cases.append(write_case("rate_135", [bytes(range(135))], 64, out / "cases"))
    cases.append(write_case("rate_136", [bytes(range(136))], 64, out / "cases"))
    cases.append(write_case("rate_137", [bytes(range(137))], 64, out / "cases"))
    cases.append(
        write_case(
            "batch_mixed",
            [b"", b"abc", bytes(range(64)), bytes(range(137))],
            64,
            out / "cases",
        )
    )

    # 从环境变量选择本次实际跑的用例（默认 abc），未知用例名直接报错阻断
    active = os.environ.get("SHAKE256_CASE", "abc")
    active_dir = out / "cases" / active
    if not active_dir.is_dir():
        raise SystemExit(f"unknown SHAKE256_CASE={active}")

    input_dir = out / "input"
    output_dir = out / "output"
    input_dir.mkdir(exist_ok=True)
    output_dir.mkdir(exist_ok=True)

    # 把选中用例的三个输入文件复制到 input/，golden 复制到 output/
    for fn in ("meta.bin", "x.bin", "lengths.bin"):
        (input_dir / fn).write_bytes((active_dir / fn).read_bytes())
    (output_dir / "golden_y.bin").write_bytes((active_dir / "golden_y.bin").read_bytes())

    manifest = {"active": active, "rate_bytes": 136, "cases": cases}
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")

    # 设备核不搬运 GM 上的 x/y，把当前用例数据编译进设备程序：
    # 调用 scripts/emit_toy_active_case_h.py 把 x/lengths/golden_y 转成 C++ 常量数组头文件
    sys.path.insert(0, str(out / "scripts"))
    from emit_toy_active_case_h import emit_toy_active_case_h  # noqa: E402

    emit_toy_active_case_h(active_dir, out / "auto_gen" / "toy_active_case.h", ns="Shake256ToyActive")
    print(f"[gen_data] SHAKE256_CASE={active} rate=136")
    print(f"[gen_data] wrote auto_gen/toy_active_case.h")
    print(f"[gen_data] wrote input/ + output/golden_y.bin (Python shake_256)")


if __name__ == "__main__":
    main()
