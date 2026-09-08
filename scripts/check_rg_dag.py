#!/usr/bin/env python3
"""Minimal DAG check for reasoning-graph YAML when rg_validate is unavailable."""
from __future__ import annotations

import argparse
import sys
from collections import defaultdict
from pathlib import Path

import yaml


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--yaml", required=True)
    args = ap.parse_args()
    g = yaml.safe_load(Path(args.yaml).read_text(encoding="utf-8"))
    nodes = g.get("nodes") or []
    ids = {n["id"] for n in nodes}
    bad = []
    for n in nodes:
        for d in n.get("deps") or []:
            if d not in ids:
                bad.append(f"{n['id']} deps missing {d}")
        for t in n.get("targets") or []:
            if t not in ids:
                bad.append(f"{n['id']} targets missing {t}")
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
    print(f"OK: {args.yaml} nodes={len(ids)} DAG")
    return 0


if __name__ == "__main__":
    sys.exit(main())
