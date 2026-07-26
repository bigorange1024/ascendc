# STATUS — pass-fix-f203-alg11-12-multiply-inner-k3

| 项 | 状态 |
|----|------|
| 阶段 | **W1/B6 实现中** |
| 结构 | `multiply/` + `inner/`（默认 P_OUT=S_VEC=3，AIV 2+1） |
| 参数卡 §3.1 | 已锁 |
| CPU | 待验 |
| SIM | 待验 |

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```
