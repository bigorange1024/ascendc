# STATUS — pass-fix-f203-stage123-ntt-intt-polyvec6-k3

| 项 | 状态 |
|----|------|
| 阶段 | **W1/B5 实现中** |
| 形状 | src/dst `[6,256]`；S0 `[12,256]`；mat_c `[48,128]`；AIV `{0,1,2}`\|`{3,4,5}` |
| 参数卡 §3.1 | 已锁 |
| CPU | 待验 |
| SIM | 待验 |

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```
