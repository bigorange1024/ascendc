# STATUS — pass-fix-f203-alg11-12-multiply-inner-k2

| 项 | 状态 |
|----|------|
| 阶段 | **W1/B6 有条件完成** |
| 结构 | `multiply/`（Alg.11）+ `inner/`（行 18；`P_OUT=S_VEC=2`；`blockDim=2`，AIV **1+1**） |
| 参数锁 | k=2；MultiplyNTTs + InnerProduct；内积输出 `t_hat[2,256]`；禁保留 k=3 的 3 行 |
| AIV split | AIV0 → `t_hat[0]`，AIV1 → `t_hat[1]`；不使用 `P_OUT/2` 临时 hack |
| CPU | **PASS**（multiply + inner；`t_hat[2,256] max=0`） |
| SIM | **PASS**（`SIM_DIRECT=1`；根无 stray dump） |

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

## SIM Total tick（910B4，Cloud 2026-07-27）

| 子项 | Total tick | 说明 |
|------|------------|------|
| `multiply/`（Alg.11） | **9290** | 单对 |
| `inner/`（行 18，1+1） | **12603** | `t_hat[2,256]` |
