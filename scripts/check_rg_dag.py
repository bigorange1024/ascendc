#!/usr/bin/env python3
"""校验工程推理图谱：委托 thirdparty/reasoning-graph-skill 的 rg_validate。

若 skill 未解压，回退到本文件内嵌的最小 DAG 检查（仅拓扑）。
"""
from __future__ import annotations

import argparse
import importlib.util
import sys
from collections import defaultdict
from pathlib import Path

import yaml

REPO = Path(__file__).resolve().parents[1]
SKILL_VALIDATE = REPO / "thirdparty/reasoning-graph-skill/scripts/rg_validate.py"


def _fallback_dag(yaml_path: Path) -> int:
    g = yaml.safe_load(yaml_path.read_text(encoding="utf-8"))
    nodes = g.get("nodes") or []
    ids = {n["id"] for n in nodes}
    bad = []
    for n in nodes:
        for d in n.get("deps") or []:
            if d not in ids:
                bad.append(f"{n['id']} deps missing {d}")
    if bad:
        print("FAIL deps:\n  " + "\n  ".join(bad))
        return 1
    indeg: dict[str, int] = defaultdict(int)
    adj: dict[str, list[str]] = defaultdict(list)
    for n in nodes:
        indeg.setdefault(n["id"], 0)
        for d in n.get("deps") or []:
            adj[d].append(n["id"])
            indeg[n["id"]] += 1
    q = [i for i in ids if indeg[i] == 0]
    seen = 0
    while q:
        u = q.pop()
        seen += 1
        for v in adj[u]:
            indeg[v] -= 1
            if indeg[v] == 0:
                q.append(v)
    if seen != len(ids):
        print(f"FAIL cycle: seen={seen} n={len(ids)}")
        return 1
    print(
        f"WARN: {SKILL_VALIDATE} 缺失，仅做拓扑检查 OK nodes={len(ids)}\n"
        f"      请解压 thirdparty/reasoning-graph-skill-master.zip → thirdparty/reasoning-graph-skill/"
    )
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--yaml",
        default=str(REPO / "docs/rg-ascendc-engineering.yaml"),
        help="推理图谱 yaml",
    )
    args = ap.parse_args()
    yaml_path = Path(args.yaml)
    if not SKILL_VALIDATE.is_file():
        return _fallback_dag(yaml_path)

    spec = importlib.util.spec_from_file_location("rg_validate", SKILL_VALIDATE)
    mod = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(mod)
    # 复用 skill CLI 入口语义
    sys.argv = ["rg_validate.py", "--yaml", str(yaml_path)]
    return int(mod.main())


if __name__ == "__main__":
    raise SystemExit(main())
