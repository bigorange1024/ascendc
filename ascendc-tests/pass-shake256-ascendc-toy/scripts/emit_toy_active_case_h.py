#!/usr/bin/env python3
# coding=utf-8
"""由 cases/<active>/ 生成 auto_gen/toy_active_case.h。"""
from __future__ import annotations

import struct
from pathlib import Path


def emit_toy_active_case_h(case_dir: Path, out_h: Path, ns: str = "Shake256ToyActive") -> None:
    meta = struct.unpack("<III", (case_dir / "meta.bin").read_bytes())
    batch, max_msg_len, out_len = meta
    x = (case_dir / "x.bin").read_bytes()
    lengths = list(struct.unpack(f"<{batch}I", (case_dir / "lengths.bin").read_bytes()))
    golden = (case_dir / "golden_y.bin").read_bytes()
    y_bytes = batch * out_len
    if len(x) != batch * max_msg_len:
        raise SystemExit("x.bin size mismatch")
    if len(golden) != y_bytes:
        raise SystemExit("golden_y.bin size mismatch")

    out_h.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "#pragma once",
        "#include <cstdint>",
        "",
        f"namespace {ns} {{",
        "",
        f"constexpr uint32_t kBatch = {batch}U;",
        f"constexpr uint32_t kMaxMsgLen = {max_msg_len}U;",
        f"constexpr uint32_t kOutLen = {out_len}U;",
        f"constexpr uint32_t kYBytes = {y_bytes}U;",
        f"constexpr uint32_t kXBytes = {len(x)}U;",
        "",
        f"constexpr uint32_t kLengths[{batch}] = {{",
        ", ".join(f"{v}U" for v in lengths),
        "};",
        "",
        f"constexpr uint8_t kX[{len(x)}] = {{",
        ", ".join(f"0x{b:02x}U" for b in x),
        "};",
        "",
        f"constexpr uint8_t kGoldenY[{len(golden)}] = {{",
        ", ".join(f"0x{b:02x}U" for b in golden),
        "};",
        "",
        f"}}  // namespace {ns}",
        "",
    ]
    out_h.write_text("\n".join(lines), encoding="utf-8")
