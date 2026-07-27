# STATUS — pass-fix-f203-byteencode-decode-d-k3

| 项 | 状态 |
|----|------|
| 阶段 | **W0/B2 有条件完成**（CPU+SIM 金标） |
| P | P2 |
| W | W0 |
| ID | B2 |
| 结构 | `encode/`+`decode/`（d=4,10）+ `encode12/`（d=12 encode-only） |
| 参数卡 | [fips203-mlkem768-parameter-card.md](../../../../docs/specs/fips203-mlkem768-parameter-card.md) |
| CPU | **PASS**（2026-07-26 Cloud） |
| SIM | **PASS**（`SIM_DIRECT=1`；根无 stray dump） |

## 验收命令

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

## SIM Total tick（910B4，Cloud 2026-07-26）

| 子项 | tick |
|------|------|
| encode d=4 | 5423 |
| decode d=4 | 9351 |
| encode d=10 | 6539 |
| decode d=10 | 6572 |
| encode12（d=12，k4 几何复用 v2 golden） | 17511 |

## 备注

- **encode12** 仍复用 1024 Tag5T v2 的 Alg.13 mixPass=7 几何（k=4 / 4×384B）；仅作 d=12 **算法**探针。768 KeyGen 的 k=3 打包在后续 D13/KeyGen 锁定。
- `decode/scripts/gen_data.py` 链接同树 `../encode`；`encode12/scripts/gen_data.py` walk-up 定位 `ml-kem-1024/.../vec-k4-v2`。
