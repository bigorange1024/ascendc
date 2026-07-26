# STATUS — pass-fix-f203-alg7-sample-ntt-k3

| 项 | 状态 |
|----|------|
| 阶段 | **W1/B4 实现中** |
| 波次 | W1 / B4 |
| 参数 | k=3；derand `…-k3:SEED_D=`；单 poly；验收矩阵 (j,i)∈{0,1,2}² |
| 参数卡 §3.1 | 已锁 |
| CPU | 待验 |
| SIM | 待验 |

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
bash run_matrix.sh -r cpu -v Ascend910B4
```
