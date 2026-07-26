#!/usr/bin/env python3
# coding=utf-8
"""兼容薄封装：转调 library/shared/fips203_host_rng（保留旧 import 路径）。"""
from __future__ import annotations

import sys
from pathlib import Path

def _ascendc_repo_root(start: Path) -> Path:
    """自 start 向上查找含 AGENTS.md 与 scripts/ 的仓库根（兼容 ml-kem 参数组嵌套）。"""
    p = start.resolve()
    for d in [p, *p.parents]:
        if (d / "AGENTS.md").is_file() and (d / "scripts").is_dir():
            return d
    raise RuntimeError(f"cannot locate ascendc repo root from {start}")


_REPO = _ascendc_repo_root(Path(__file__).resolve())
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
