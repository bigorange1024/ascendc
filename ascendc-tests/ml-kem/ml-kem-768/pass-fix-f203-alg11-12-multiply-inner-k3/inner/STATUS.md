# pass-fix-f203-alg11-12-innerproduct-k4-halfrows

**4×4×1 半行内积**（双 AIV，各 2 行）— **v2 行 18 内积的上游单用例**。

- GM：**全量** `a_hat` / `s_hat`（alg13 行主序 `(p*K+j)*N`）
- **blockDim=2**：AIV0 → `t̂[0:2]`，AIV1 → `t̂[2:4]`（与 v2 `pBegin_/pEnd_` 分工同构）
- j→p + 全 poly `compute_on_ub`；`ALG11_MEM_OPS=1` ROM DataCopy

| 文档 | 路径 |
|------|------|
| 方案 | [INNERPRODUCT_K4_HALFROWS_PLAN.md](INNERPRODUCT_K4_HALFROWS_PLAN.md) |
| SIM 日志 | [SIM_BENCHMARK.md](SIM_BENCHMARK.md) |
| v2 集成 | [vec-k4-v2/SIM_BENCHMARK.md](../pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/SIM_BENCHMARK.md) |
| 技术总结 | [F203-innerproduct-k4-技术总结.md](../../docs/notes/F203-innerproduct-k4-技术总结.md) |

## 默认宏（`bash run.sh`）

`ALG11_IMPL=1` `ALG11_VEC_VARIANT=2` `ALG11_VEC_OPTS=1` `ALG11_MEM_OPS=1` · `INNERPRODUCT_P_OUT=4` `INNERPRODUCT_S_VEC=4`

## 验收（Ascend910B4，2026-06-19 复测）

| 模式 | 结果 | SIM Total tick |
|------|------|----------------|
| cpu | PASS | — |
| sim | PASS | **26185** |

```bash
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```

## 与 v2 融合对照

| 场景 | SIM tick | 说明 |
|------|----------|------|
| **本探针**（仅内积） | **26185** | 独立 launch；GM 读 `a_hat`/`s_hat` |
| v2 dot-only 边际 | +20840（65488−44648） | 含 NTT 后 UB、单 TPipe、Alg11 ROM 等；**不可与 26185 直接比绝对值** |

全量单 AIV：[`innerproduct-k4`](../pass-fix-f203-alg11-12-innerproduct-k4/)（SIM **43992**）
