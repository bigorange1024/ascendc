#!/usr/bin/env python3
"""渲染工程推理图谱 HTML：委托 reasoning-graph-skill 的 rg_render + 官方 rg-viewer.html。

这才是授权第三方仓的可视化（分层 L0↑、圆/三角/菱形/方框、domain 着色），
不是自造 Cytoscape 模板，也不是 cannbot-knowledge 的 okf 图。

Usage:
  python3 scripts/rg_viz.py
  python3 scripts/rg_viz.py --yaml docs/rg-ascendc-engineering.yaml \\
      --out docs/rg-ascendc-engineering.viz.html
"""
from __future__ import annotations

import argparse
import importlib.util
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
SKILL_ROOT = REPO / "thirdparty/reasoning-graph-skill"
SKILL_RENDER = SKILL_ROOT / "scripts/rg_render.py"
SKILL_VIEWER = SKILL_ROOT / "assets/rg-viewer.html"
DEFAULT_YAML = REPO / "docs/rg-ascendc-engineering.yaml"
DEFAULT_OUT = REPO / "docs/rg-ascendc-engineering.viz.html"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--yaml", type=Path, default=DEFAULT_YAML)
    ap.add_argument("--out", type=Path, default=DEFAULT_OUT)
    ap.add_argument("--force", action="store_true", help="校验失败仍渲染")
    args = ap.parse_args()

    if not SKILL_RENDER.is_file() or not SKILL_VIEWER.is_file():
        print(
            "ERROR: 未找到 reasoning-graph-skill。\n"
            f"  期望: {SKILL_ROOT}/\n"
            "  安装: unzip thirdparty/reasoning-graph-skill-master.zip "
            "-d thirdparty/ && mv …/reasoning-graph-skill-master "
            "thirdparty/reasoning-graph-skill\n"
            "  详见 docs/engineering/thirdparty-本地依赖.md",
            file=sys.stderr,
        )
        return 2

    spec = importlib.util.spec_from_file_location("rg_render", SKILL_RENDER)
    mod = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(mod)
    out = mod.build(args.yaml, SKILL_VIEWER, args.out, force=args.force)
    print(f"OK: {out} (template={SKILL_VIEWER.name})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
