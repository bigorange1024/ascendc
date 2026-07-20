#!/usr/bin/env python3
"""
verify_result.py — Decrypt 生产验收：output/m.bin vs output/golden_m.bin。

对齐 FIPS 203 Alg.15 输出 m（32B）；仅验 I/O 字节一致，不对照设备实现细节。
由 run.sh 在 kernel 成功后调用。
"""
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
