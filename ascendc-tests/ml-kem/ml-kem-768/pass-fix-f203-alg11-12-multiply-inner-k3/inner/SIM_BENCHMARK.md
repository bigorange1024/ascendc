# SIM 验收日志 — pass-fix-f203-alg11-12-innerproduct-k4-halfrows

Ascend910B4 camodel · **4×4×1** · `blockDim=2` · 默认 `ALG11_VEC_OPTS=1` `ALG11_MEM_OPS=1`

| 日期 | 布局 | Total tick | 对拍 | 备注 |
|------|------|------------|------|------|
| 2026-06-17 | `a_col` 列主序 | ~25638 | PASS | **已废弃布局** |
| 2026-06-18 | `a_hat` 行主序 | ~26230 | PASS | 首次行主序定标 |
| **2026-06-19** | **`a_hat` 行主序** | **26185** | PASS | v2 性能表同步复测；camodel ±~10 tick 正常 |

**v2 合入对照**：独立探针 **26185**；融合 NTT+行18 dot-only **65488**（见 [vec-k4-v2/SIM_BENCHMARK.md](../pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/SIM_BENCHMARK.md)）。

全量单 AIV：见 [innerproduct-k4/SIM_BENCHMARK.md](../pass-fix-f203-alg11-12-innerproduct-k4/SIM_BENCHMARK.md)。

```bash
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```
