"""固定 Kyber poly（seed=42），供 vec-k4-v2 探针 golden 与 NTT 对拍。"""
from __future__ import annotations

import numpy as np

Q = 3329
_SEED = 42
_rng = np.random.default_rng(_SEED)
FIXED_POLY = _rng.integers(0, Q, size=256, dtype=np.int32)
FIXED_E_POLY = np.random.default_rng(_SEED + 1).integers(0, Q, size=256, dtype=np.int32)
