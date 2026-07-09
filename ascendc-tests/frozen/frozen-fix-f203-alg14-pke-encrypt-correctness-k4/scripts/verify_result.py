#!/usr/bin/env python3
"""verify — output/c.bin vs output/golden_c.bin（字节级）。"""
import sys
from pathlib import Path
import numpy as np

CASE = Path(__file__).resolve().parent.parent
out = CASE / "output"
c = np.fromfile(out / "c.bin", dtype=np.uint8)
g = np.fromfile(out / "golden_c.bin", dtype=np.uint8)
if c.shape != g.shape:
    print(f"[verify] FAIL shape {c.shape} vs {g.shape}")
    sys.exit(1)
diff = np.abs(c.astype(np.int16) - g.astype(np.int16))
mx = int(diff.max()) if diff.size else 0
if mx != 0:
    idx = int(diff.argmax())
    print(f"[verify] FAIL max={mx} at {idx} c={c[idx]} g={g[idx]}")
    sys.exit(1)
print(f"[verify] PASS max=0 ({c.size} bytes)")
