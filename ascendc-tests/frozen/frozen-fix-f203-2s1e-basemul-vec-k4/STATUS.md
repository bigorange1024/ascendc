# frozen-fix-f203-2s1e-basemul-vec-k4

⛔ **已冻结** — 见 [FROZEN.md](FROZEN.md)。**禁止参考、fork 或合并。**

行 18 `MultiplyNTTs` 向量化 spike（2026-06-15 晚间）。基于 [byteencode12-vec-k4](../../pass-fix-f203-2s1e-byteencode12-vec-k4/)。

| `HAT_BASEMUL_VARIANT` | CPU | SIM | SIM 性能 vs 标量 |
|------------------------|-----|-----|------------------|
| 0 标量 | ✓ | ✓ | 基线 ~50s |
| 1 deinterleave+vec | ✓ | ✓ | **更慢 ~80s** |
| 2 gather+vec | ✓ | ✓ | **更慢 ~69s** |

**放弃原因**：性能负收益 + 混合 Barrett；继任 [`pass-fix-f203-alg11-12-multiplyntts-k4`](../../pass-fix-f203-alg11-12-multiplyntts-k4/)。

方案存档：[BASEMUL_VEC.md](BASEMUL_VEC.md)
