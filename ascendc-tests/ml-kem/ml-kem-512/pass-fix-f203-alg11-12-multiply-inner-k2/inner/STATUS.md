# pass-fix-f203-alg11-12-multiply-inner-k2/inner

ML-KEM-512 **2×2×1 行 18 InnerProduct**，双 AIV 各写 1 行。

- GM：`a_hat[2,2,256]` / `s_hat[2,256]` / `t_hat[2,256]`。
- `blockDim=2`：AIV0 → `t_hat[0]`，AIV1 → `t_hat[1]`。
- j→p + 全 poly `compute_on_ub`；`ALG11_MEM_OPS=1` ROM DataCopy。
- 分片说明：[INNERPRODUCT_K2_SPLIT_PLAN.md](INNERPRODUCT_K2_SPLIT_PLAN.md)。

## 默认宏（`bash run.sh`）

`ALG11_IMPL=1` `ALG11_VEC_VARIANT=2` `ALG11_VEC_OPTS=1` `ALG11_MEM_OPS=1` · `INNERPRODUCT_P_OUT=2` `INNERPRODUCT_S_VEC=2`

## 验收

| 模式 | 结果 | SIM Total tick |
|------|------|----------------|
| cpu | **PASS** | — |
| sim | **PASS** | **12603** |

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```
