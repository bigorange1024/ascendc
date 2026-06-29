#!/usr/bin/env python3
# coding=utf-8
"""SHAKE256 golden：Python hashlib.shake_256 + FIPS PRF 形参 (σ||N, 128B)。"""
from __future__ import annotations

import hashlib
import json
import os
import struct
import sys
from pathlib import Path

SEED_D_DEFAULT = 20260619
K = 4


def derand_and_sigma(seed_d: int) -> bytes:
    msg = f"exp-mlkem-f203-2s1e-k4:SEED_D={seed_d}".encode()
    d = hashlib.sha3_256(msg).digest()
    buf = hashlib.sha3_512(d + bytes([K & 0xFF])).digest()
    return buf[32:64]


def write_case(name: str, messages: list[bytes], out_len: int, root: Path) -> dict:
    batch = len(messages)
    max_msg_len = max((len(m) for m in messages), default=0)
    if max_msg_len == 0:
        max_msg_len = 1

    x = bytearray(batch * max_msg_len)
    lengths = []
    golden = bytearray()
    for i, msg in enumerate(messages):
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
    seed_d = int(os.environ.get("SEED_D", str(SEED_D_DEFAULT)))
    sigma = derand_and_sigma(seed_d)
    prf_msg = sigma + bytes([0])

    out = Path(__file__).resolve().parent
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

    active = os.environ.get("SHAKE256_CASE", "abc")
    active_dir = out / "cases" / active
    if not active_dir.is_dir():
        raise SystemExit(f"unknown SHAKE256_CASE={active}")

    input_dir = out / "input"
    output_dir = out / "output"
    input_dir.mkdir(exist_ok=True)
    output_dir.mkdir(exist_ok=True)

    for fn in ("meta.bin", "x.bin", "lengths.bin"):
        (input_dir / fn).write_bytes((active_dir / fn).read_bytes())
    (output_dir / "golden_y.bin").write_bytes((active_dir / "golden_y.bin").read_bytes())

    manifest = {"active": active, "rate_bytes": 136, "cases": cases}
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")

    sys.path.insert(0, str(out / "scripts"))
    from emit_toy_active_case_h import emit_toy_active_case_h  # noqa: E402

    emit_toy_active_case_h(active_dir, out / "auto_gen" / "toy_active_case.h", ns="Shake256ToyActive")
    print(f"[gen_data] SHAKE256_CASE={active} rate=136")
    print(f"[gen_data] wrote auto_gen/toy_active_case.h")
    print(f"[gen_data] wrote input/ + output/golden_y.bin (Python shake_256)")


if __name__ == "__main__":
    main()
