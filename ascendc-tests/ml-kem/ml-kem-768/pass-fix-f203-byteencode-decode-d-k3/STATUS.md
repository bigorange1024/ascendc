# STATUS — pass-fix-f203-byteencode-decode-d-k3

| 项 | 状态 |
|----|------|
| 阶段 | **W0/B2 实现中** |
| 波次 | W0 / B2 |
| 结构 | `encode/`+`decode/`（d=4,10）+ `encode12/`（d=12 encode-only） |
| 参数卡 | [fips203-mlkem768-parameter-card.md](../../../../docs/specs/fips203-mlkem768-parameter-card.md) |
| CPU | 待验 |
| SIM | 待验 |

## 验收命令

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```
