# 半行 AIV 内积（已实现）

**目录**：`pass-fix-f203-alg11-12-innerproduct-k4-halfrows`  
**前置**：[`pass-fix-f203-alg11-12-innerproduct-k4`](../pass-fix-f203-alg11-12-innerproduct-k4/) 4×4×1 全量单 AIV 已验收。

## GM 布局（与 alg13 一致）

```text
a_hat.bin : flat(p,j,c) = (p * K + j) * N + c
s_hat.bin : flat(j,c)   = j * N + c
```

**禁止** `a_col` 列主序转置。

## 双 AIV 分工

| AIV | 输出行 |
|-----|--------|
| 0 | t̂[0], t̂[1] |
| 1 | t̂[2], t̂[3] |

- `blockDim=2`，`pBegin = blockIdx * 2`
- 循环：外层 j、内层 p（本 AIV 行区间）
- 每 (p,j)：`DataCopy` from `a_hat_offset(p,j)` + `compute_on_ub`

## 验收

| 模式 | 结果 | SIM tick（`a_hat` 行主序） |
|------|------|---------------------------|
| cpu | PASS | — |
| sim | PASS | **26185** |

SIM 日志：[SIM_BENCHMARK.md](SIM_BENCHMARK.md)

```bash
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```
