#!/usr/bin/env python3
"""verify — output/m.bin vs output/golden_m.bin。"""
import sys
from pathlib import Path
import numpy as np

CASE = Path(__file__).resolve().parent.parent
out = CASE / "output"
m = np.fromfile(out / "m.bin", dtype=np.uint8)
g = np.fromfile(out / "golden_m.bin", dtype=np.uint8)
if m.shape != g.shape:
    print(f"[verify] FAIL shape {m.shape} vs {g.shape}")
    sys.exit(1)
diff = np.abs(m.astype(np.int16) - g.astype(np.int16))
mx = int(diff.max()) if diff.size else 0
if mx != 0:
    idx = int(diff.argmax())
    print(f"[verify] FAIL max={mx} at {idx} m={m[idx]} g={g[idx]}")
    sys.exit(1)
print(f"[verify] PASS max=0 ({m.size} bytes)")
