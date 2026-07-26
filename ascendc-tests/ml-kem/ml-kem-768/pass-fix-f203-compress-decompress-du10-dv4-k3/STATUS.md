# STATUS — pass-fix-f203-compress-decompress-du10-dv4-k3

| 项 | 状态 |
|----|------|
| 阶段 | **W0/B1 实现中** |
| 波次 | W0 / B1 |
| 结构 | 子目录 `compress/` + `decompress/`；顶层 `run.sh` 编排 d∈{4,10} |
| 参数卡 | [fips203-mlkem768-parameter-card.md](../../../../docs/specs/fips203-mlkem768-parameter-card.md) |
| CPU | 待验 |
| SIM | 待验 |

## 验收命令

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```
