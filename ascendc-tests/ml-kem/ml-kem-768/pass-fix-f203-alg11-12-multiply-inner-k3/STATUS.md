# STATUS — pass-fix-f203-alg11-12-multiply-inner-k3

| 项 | 状态 |
|----|------|
| 阶段 | **W1/B6 有条件完成** |
| 结构 | `multiply/`（Alg.11）+ `inner/`（行 18；P_OUT=S_VEC=3；`blockDim=2`，AIV **2+1**） |
| 参数卡 §3.1 | 已锁（禁 `P_OUT/2`） |
| CPU | **PASS**（multiply + inner；`t_hat[3,256] max=0`） |
| SIM | **PASS**（`SIM_DIRECT=1`；根无 stray dump） |

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

## SIM Total tick（910B4，Cloud 2026-07-26）

| 子项 | Total tick | 说明 |
|------|------------|------|
| `multiply/`（Alg.11） | **9416** | 单对；登记见 [`qa/active_sim_regress_summary.md`](../../../../qa/active_sim_regress_summary.md) |
| `inner/`（行 18，2+1） | **21881** | `t_hat[3,256]` |
