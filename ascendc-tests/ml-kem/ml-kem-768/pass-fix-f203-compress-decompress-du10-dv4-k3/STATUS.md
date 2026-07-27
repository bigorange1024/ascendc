# STATUS — pass-fix-f203-compress-decompress-du10-dv4-k3

| 项 | 状态 |
|----|------|
| 阶段 | **W0/B1 有条件完成**（CPU+SIM 金标） |
| P | P2 |
| W | W0 |
| ID | B1 |
| 结构 | 子目录 `compress/` + `decompress/`；顶层 `run.sh` 编排 d∈{4,10} |
| 参数卡 | [fips203-mlkem768-parameter-card.md](../../../../docs/specs/fips203-mlkem768-parameter-card.md) |
| CPU | **PASS**（2026-07-26 Cloud） |
| SIM | **PASS**（`SIM_DIRECT=1`；根无 stray dump） |

## 验收命令

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

## SIM Total tick（910B4，Cloud 2026-07-26）

| 子项 | d=4 | d=10 |
|------|-----|------|
| compress | 3196 | 3420 |
| decompress | 3304 | 3247 |

## 备注

- 768 密文域仅强制验收 **d∈{4,10}**（\(d_v=4,d_u=10\)）；子探针内核仍可编 d=5/11（1024 对照，非本波默认矩阵）。
