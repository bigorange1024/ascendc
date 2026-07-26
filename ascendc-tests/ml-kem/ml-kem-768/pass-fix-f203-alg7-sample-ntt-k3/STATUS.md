# STATUS — pass-fix-f203-alg7-sample-ntt-k3

| 项 | 状态 |
|----|------|
| 阶段 | **W1/B4 有条件完成** |
| 波次 | W1 / B4 |
| 参数 | k=3；derand `…-k3:SEED_D=`；单 poly；验收矩阵 (j,i)∈{0,1,2}² |
| 参数卡 §3.1 | 已锁（`blockDim=1`；G 内 k=3） |
| CPU | **PASS**（默认 (0,0)；`run_matrix.sh` 全 3×3 亦 PASS） |
| SIM | **PASS**（默认 (0,0)；`SIM_DIRECT=1`；Total tick **80783**；根无 stray dump） |

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
bash run_matrix.sh -r cpu -v Ascend910B4
```
