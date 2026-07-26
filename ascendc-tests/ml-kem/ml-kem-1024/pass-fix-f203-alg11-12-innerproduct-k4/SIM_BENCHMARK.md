# SIM 验收日志 — pass-fix-f203-alg11-12-innerproduct-k4

Ascend910B4 camodel · **4×4×1** · 单 AIV · 默认 `ALG11_VEC_OPTS=1` `ALG11_MEM_OPS=1`

| 日期 | 布局 | P×S | Total tick | 对拍 | 备注 |
|------|------|-----|------------|------|------|
| 2026-06-17 | `a_col` 列主序 | 4×4 | ~40805 | PASS | **已废弃布局** |
| 2026-06-18 | `a_hat` 行主序 | 4×4 | ~43932 | PASS | 首次行主序定标 |
| **2026-06-19** | **`a_hat` 行主序** | 4×4 | **43992** | PASS | v2 性能表同步复测 |

半行双 AIV（**26185**）：[halfrows/SIM_BENCHMARK.md](../pass-fix-f203-alg11-12-innerproduct-k4-halfrows/SIM_BENCHMARK.md)  
v2 融合 dot-only 边际（65488−44648）：见 [vec-k4-v2/SIM_BENCHMARK.md](../pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/SIM_BENCHMARK.md)

```bash
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```
