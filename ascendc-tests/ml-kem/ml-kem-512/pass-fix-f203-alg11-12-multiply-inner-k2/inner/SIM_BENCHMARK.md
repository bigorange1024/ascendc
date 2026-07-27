# SIM 验收日志 — pass-fix-f203-alg11-12-multiply-inner-k2/inner

Ascend910B4 CAModel · **2×2×1** · `blockDim=2` · AIV **1+1** · 默认 `ALG11_VEC_OPTS=1` `ALG11_MEM_OPS=1`

| 日期 | 布局 | Total tick | 对拍 | 备注 |
|------|------|------------|------|------|
| 2026-07-27 | `a_hat` 行主序 | **12603** | PASS | 新建 k2 B6；`t_hat[2,256]` max=0 |

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```
