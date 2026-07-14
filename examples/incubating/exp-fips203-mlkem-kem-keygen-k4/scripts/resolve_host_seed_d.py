#!/usr/bin/env python3
# coding=utf-8
"""兼容薄封装：转调 library/shared/fips203_host_rng（保留旧 import 路径）。"""
from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(_REPO / "library/shared/fips203_host_rng"))
from host_rng import resolve_seed_d as _resolve  # noqa: E402

_CASE_TAG = "fips203-mlkem-kem-keygen-k4"


def resolve_host_seed_d() -> tuple[int, str]:
    return _resolve(_CASE_TAG)


if __name__ == "__main__":
    import os

    sd, how = resolve_host_seed_d()
    if os.environ.get("SEED_D_RESOLVE_VERBOSE", "0") == "1":
        print(f"[resolve_host_seed_d] SEED_D={sd} via {how}", file=sys.stderr)
    print(sd)
