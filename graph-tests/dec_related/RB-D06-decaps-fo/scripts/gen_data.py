#!/usr/bin/env python3
"""gen_data — RB-D06-decaps-fo：合法 + 拒绝两路径 FO 中间量与权威 golden_K。

本脚本是黑盒 oracle / 向量工厂，不是 AscendC 实现规格。

权威门禁（TASK / S0B §2.3）：
  - KeyGen / Encaps / Decaps 一律优先 `scripts/liboqs_kem_ref`（liboqs）。
  - 缺库 → 写 output/BLOCKED + exit 2（禁止 python Decaps / 自写 J 冒充权威）。
  - hashlib.shake_256 仅作拒绝路径诊断对照，不写入 golden。

设备输入（Host 造中间量；本刀不跑 Decrypt/ReEnc）：
  合法：c'=c；K'=Encaps 的 K（合法时 G 输出与 Encaps K 一致）；z←dk[3136:3168)
  拒绝：c 翻末字节；c' 仍为合法原 c；K'/z 不变 → 迫使 FO 走 J(z‖c_bad)

假绿防护：golden_reject 必须 ≠ golden_legit；verify 再断言设备两侧不同。
"""
from __future__ import annotations

import hashlib
import os
import sys
from pathlib import Path

_SCRIPT = Path(__file__).resolve().parent
_CASE = _SCRIPT.parent
_REPO = _CASE.parents[2]

sys.path.insert(0, str(_REPO / "library" / "shared" / "f203_kem_ref"))
from kem_ref import (  # noqa: E402
    kem_decaps,
    kem_encaps,
    kem_keygen,
    liboqs_ref,
)

HALF = 32
CT = 1568
DK = 3168
Z_OFF = 3136
Z_END = 3168

# 可复现非全 0：kem_seed=d‖z；m 非全 0（Encaps 禁默认可全 0）
FIXED_KEM_SEED = bytes([(0x29 + (i * 19)) & 0xFF for i in range(64)])
FIXED_M = bytes([(0xC3 ^ (i * 23)) & 0xFF for i in range(HALF)])


def _blocked(out: Path, msg: str) -> int:
    """缺权威库：写 BLOCKED 并返回 2。"""
    out.mkdir(parents=True, exist_ok=True)
    (out / "BLOCKED").write_text(msg + "\n", encoding="utf-8")
    (out / "cross_backend.txt").write_text("missing\n", encoding="utf-8")
    print(f"[BLOCKED] {msg}", file=sys.stderr)
    return 2


def _write_path(indir: Path, c: bytes, c_prime: bytes, k_prime: bytes, z: bytes) -> None:
    """落盘单路径设备输入。"""
    indir.mkdir(parents=True, exist_ok=True)
    (indir / "c.bin").write_bytes(c)
    (indir / "c_prime.bin").write_bytes(c_prime)
    (indir / "k_prime.bin").write_bytes(k_prime)
    (indir / "z.bin").write_bytes(z)


def main() -> int:
    out = _CASE / "output"
    inp = _CASE / "input"
    out.mkdir(parents=True, exist_ok=True)
    inp.mkdir(parents=True, exist_ok=True)

    if liboqs_ref() is None:
        return _blocked(
            out,
            "缺 liboqs / scripts/liboqs_kem_ref；K03 FO 权威 Decaps 不可用。"
            "请 bash scripts/clone-thirdparty.sh && bash scripts/build_liboqs_kem_ref.sh",
        )

    ek_path = out / "_ek_kem.bin"
    dk_path = out / "_dk_kem.bin"
    c_path = out / "_c_legit.bin"
    k_enc_path = out / "_k_encaps.bin"

    src_kg = kem_keygen(FIXED_KEM_SEED, ek_path, dk_path)
    if src_kg != "liboqs":
        return _blocked(out, f"kem_keygen 回落为 {src_kg!r}（须 liboqs）")

    ek = ek_path.read_bytes()
    dk = dk_path.read_bytes()
    if len(dk) != DK or len(ek) != CT:
        return _blocked(out, f"密钥尺寸异常 ek={len(ek)} dk={len(dk)}")

    src_enc = kem_encaps(ek, FIXED_M, c_path, k_enc_path, ek_path=ek_path)
    if src_enc != "liboqs":
        return _blocked(out, f"kem_encaps 回落为 {src_enc!r}（须 liboqs）")

    c = c_path.read_bytes()
    k_enc = k_enc_path.read_bytes()
    z = dk[Z_OFF:Z_END]
    assert len(c) == CT and len(k_enc) == HALF and len(z) == HALF

    # 权威 Decaps（合法）
    k_legit_path = out / "golden_k_legit.bin"
    src_dec_ok = kem_decaps(dk, c, k_legit_path, dk_path=dk_path, c_path=c_path)
    if src_dec_ok != "liboqs":
        return _blocked(out, f"kem_decaps(legit) 回落为 {src_dec_ok!r}")
    k_legit = k_legit_path.read_bytes()
    if k_legit != k_enc:
        return _blocked(
            out,
            f"fixture 坏：liboqs Decaps(K) != Encaps(K) "
            f"dec={k_legit[:4].hex()} enc={k_enc[:4].hex()}",
        )

    # 拒绝：确定性翻末字节；c' 保持合法原 c，迫使 c≠c'
    c_bad = bytearray(c)
    c_bad[-1] ^= 0x01
    c_bad_b = bytes(c_bad)
    c_bad_path = out / "_c_reject.bin"
    c_bad_path.write_bytes(c_bad_b)

    k_rej_path = out / "golden_k_reject.bin"
    src_dec_bad = kem_decaps(dk, c_bad_b, k_rej_path, dk_path=dk_path, c_path=c_bad_path)
    if src_dec_bad != "liboqs":
        return _blocked(out, f"kem_decaps(reject) 回落为 {src_dec_bad!r}")
    k_rej = k_rej_path.read_bytes()
    if k_rej == k_legit:
        return _blocked(out, "拒绝路径 golden_K 与合法相同（向量/权威异常）")

    # 诊断：J(z‖c_bad) 应与权威拒绝 K 一致（非权威门，仅日志）
    j_diag = hashlib.shake_256(z + c_bad_b).digest(HALF)
    j_match = j_diag == k_rej

    # K'：合法 Encaps/G 共享密钥（FO 合法选路源）；两路径共用同一 K'/z
    k_prime = k_enc

    _write_path(inp / "legit", c=c, c_prime=c, k_prime=k_prime, z=z)
    _write_path(inp / "reject", c=c_bad_b, c_prime=c, k_prime=k_prime, z=z)

    (out / "cross_backend.txt").write_text("liboqs\n", encoding="utf-8")
    (out / "oracle_note.txt").write_text(
        "authority=liboqs_kem_ref Decaps\n"
        "diag_J=hashlib.shake_256 (not authority)\n"
        f"j_diag_matches_reject_golden={j_match}\n"
        "z_source=dk_kem[3136:3168)_slice\n"
        "reject_tamper=c[-1]^=0x01; c_prime=legit_c\n"
        "forbidden=python_decaps_as_authority\n",
        encoding="utf-8",
    )
    # 清理上次失败遗留
    blocked = out / "BLOCKED"
    if blocked.is_file():
        blocked.unlink()

    print(
        f"[gen_data] backend=liboqs legit_K={k_legit[:4].hex()}… "
        f"reject_K={k_rej[:4].hex()}… j_diag_ok={j_match} "
        f"K_rej!=K_legit={k_rej != k_legit}",
        file=sys.stderr,
    )
    print(
        f"[gen_data] paths=input/legit|reject "
        f"c_head={c[:4].hex()} c_bad_tail={c_bad_b[-1]:02x}"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
