# pass-fix-f203-alg11-12-multiply-inner-k2/multiply

FIPS 203 **Alg.11** + **Alg.12** 向量核探针（单次 `MultiplyNTTs(f,g)`）。本子目录在 ML-KEM-512 W1/B6 中复用，算法本身与 k 无关。

**更新**：2026-07-27（纳入 `pass-fix-f203-alg11-12-multiply-inner-k2`）

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
| **1**（B6 k2 复测） | **9290** | `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4`，2026-07-27 |
| **1**（默认） | **~9031** | `bash scripts/ab_mem_ops.sh sim` |
| 0 | ~16359 | CreateVecIndex + 标量 interleave |

详见 [ALG11_12_VEC_PLAN.md §0](ALG11_12_VEC_PLAN.md)。

## 文档

- 探针说明：[ALG11_12.md](ALG11_12.md)
- 实现方案：[ALG11_12_VEC_PLAN.md](ALG11_12_VEC_PLAN.md)
- 2026-06-16 经验：[qa/2026-06/2026-06-16-Alg11-12向量化与微优化A-B.md](../../qa/2026-06/2026-06-16-Alg11-12向量化与微优化A-B.md)

## 在 B6 中的角色

`multiply/` 只验证单对 `MultiplyNTTs`；`inner/` 负责 k=2 的 `P_OUT=S_VEC=2` 行 18 内积。顶层 `run.sh` 会顺序跑两个子项。
