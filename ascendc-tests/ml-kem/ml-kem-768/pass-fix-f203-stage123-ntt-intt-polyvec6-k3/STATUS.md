# STATUS — pass-fix-f203-stage123-ntt-intt-polyvec6-k3

| 项 | 状态 |
|----|------|
| 阶段 | **W1/B5 有条件完成** |
| 形状 | src/dst `[6,256]`；S0 `[12,256]`；mat_c `[48,128]`；AIV 连续 `{0,1,2}`\|`{3,4,5}`；MIX `blockDim=1` |
| 参数卡 §3.1 | 已锁（Cube pad m→16 **非**假 poly） |
| CPU | **PASS**（`mode=ntt` + `mode=intt`；`max=0`） |
| SIM | **PASS**（`SIM_DIRECT=1`；NTT tick **26651** / INTT **26672**；根无 stray dump） |

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
F203_NTT_MODE=intt bash run.sh -r cpu -v Ascend910B4
F203_NTT_MODE=intt SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```
