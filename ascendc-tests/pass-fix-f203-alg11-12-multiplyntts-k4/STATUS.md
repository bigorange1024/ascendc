# pass-fix-f203-alg11-12-multiplyntts-k4

FIPS 203 **Alg.11** + **Alg.12** 向量核权威探针（单次 `MultiplyNTTs(f,g)`）。行 18 basemul 迁入 2s1e 的**基线实现**。

**更新**：2026-06-16（自 `toy-alg11-12-multiplyntts-k4` 更名）

## 模型

N=256，q=3329；**f̂,ĝ** 为 Z_q 上 varied 固定系数（`f[i]=(17i+3)%q`，`g[i]=(13i+7)%q`）；**γ[i]=kMlkemGammas[i]**（`alg11_gammas.h`）。

## 默认变体

| 宏 | 默认 | 路径 |
|----|------|------|
| `ALG11_IMPL` | `1` | 向量 |
| `ALG11_VEC_VARIANT` | `2` | B2 Gather + 全向量 Barrett |
| `ALG11_VEC_OPTS` | **`0`** | legacy Barrett 微路径 |
| `ALG11_MEM_OPS` | **`1`** | `__gm__` ROM + Init `DataCopy`（SIM tick ~−45% vs `0`） |

```bash
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
ALG11_VEC_OPTS=1 bash run.sh -r sim -v Ascend910B4   # §6 微优化对照
ALG11_MEM_OPS=0 bash run.sh -r sim -v Ascend910B4   # SetValue/CreateVecIndex 对照
bash scripts/ab_mem_ops.sh sim
ALG11_IMPL=0 bash run.sh -r cpu -v Ascend910B4
ALG11_VEC_VARIANT=1 bash run.sh -r sim -v Ascend910B4
```

## 验收

| 变体 | CPU | SIM |
|------|-----|-----|
| IMPL=1 VAR=2 OPTS=0 MEM_OPS=1（**默认**） | ✓ | ✓ |
| IMPL=1 VAR=2 MEM_OPS=0 | ✓ | ✓ |
| IMPL=1 VAR=2 OPTS=1 | ✓ | ✓ |
| IMPL=1 VAR=1 | ✓ | ✓ |
| IMPL=0 | ✓ | ✓ |

## 性能（SIM，Ascend910B4，B2，`OPTS=0`）

| `ALG11_MEM_OPS` | Total tick | 备注 |
|-----------------|------------|------|
| **1**（默认） | **~9031** | `bash scripts/ab_mem_ops.sh sim` |
| 0 | ~16359 | CreateVecIndex + 标量 interleave |

详见 [ALG11_12_VEC_PLAN.md §0](ALG11_12_VEC_PLAN.md)。

## 文档

- 探针说明：[ALG11_12.md](ALG11_12.md)
- 实现方案：[ALG11_12_VEC_PLAN.md](ALG11_12_VEC_PLAN.md)
- 2026-06-16 经验：[qa/2026-06/2026-06-16-Alg11-12向量化与微优化A-B.md](../../qa/2026-06/2026-06-16-Alg11-12向量化与微优化A-B.md)

## 与已冻结 basemul-vec 的关系

[`frozen-fix-f203-2s1e-basemul-vec-k4`](../frozen/frozen-fix-f203-2s1e-basemul-vec-k4/) 为 2s1e 全链路 spike，SIM **慢于标量**已冻结。**勿参考**；行 18 迁入以本目录 `MEM_OPS=1` 模块为准。
