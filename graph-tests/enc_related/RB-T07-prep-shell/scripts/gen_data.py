#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""RB-T07 gen_data：委托 prep_host 写固定 ek/coins 与 golden（Host 黑盒 oracle）。"""
from __future__ import annotations

import importlib.util
from pathlib import Path

SCRIPT = Path(__file__).resolve().parent / "prep_host.py"


def main() -> int:
    spec = importlib.util.spec_from_file_location("rb_t07_prep_host", SCRIPT)
    assert spec and spec.loader
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return int(mod.main())


if __name__ == "__main__":
    raise SystemExit(main())
