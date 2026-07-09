# pass-fix-f203-alg11-12-innerproduct-k4

**4×4×1 全量 polyvec NTT 域内积**（ML-KEM K=4 KeyGen 行 18，无 ê）— **v2 行 18 的上游单用例（单 AIV）**。

```text
t̂[p] = mod_q( Σ_j Â[p,j]∘ŝ[j] )    p = 0..3
```

- **GM 与 alg13 一致**：`a_hat[(p*K+j)*N+c]` 行主序
- **单 AIV**：外层 j、内层 p；16× `alg11_ub::compute_on_ub`（来自 [`multiplyntts-k4`](../pass-fix-f203-alg11-12-multiplyntts-k4/)）
- 默认 `ALG11_VEC_OPTS=1` `ALG11_MEM_OPS=1`

| 文档 | 路径 |
|------|------|
| 方案 | [INNERPRODUCT_K4_PLAN.md](INNERPRODUCT_K4_PLAN.md) |
| SIM 日志 | [SIM_BENCHMARK.md](SIM_BENCHMARK.md) |
| v2 集成 | [vec-k4-v2/SIM_BENCHMARK.md](../pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/SIM_BENCHMARK.md) |

## 运行

```bash
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
INNERPRODUCT_P_OUT=2 INNERPRODUCT_S_VEC=2 bash run.sh -r sim -v Ascend910B4
```

## 验收（Ascend910B4，2026-06-19 复测）

| 模式 | 结果 | SIM Total tick |
|------|------|----------------|
| cpu | PASS | — |
| sim（4×4×1） | PASS | **43992** |

## 半行双 AIV

[`innerproduct-k4-halfrows`](../pass-fix-f203-alg11-12-innerproduct-k4-halfrows/) — SIM **26185**（各 AIV 2 行，与 v2 分片同构）

## 二期 half

已冻结：`frozen/frozen-fix-f203-alg11-12-innerproduct-k4-halfbatch/`
