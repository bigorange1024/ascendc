# frozen-merged-kyber-ntt256-limb6-poly2-s12

> **已冻结**（2026-06-12）：merged_kyber poly2 Stage1+2；只读对照。下游 `frozen-fix-merged-kyber-…-poly2-s12` 自本目录 fork。

Phase D′ @ 6bit：**2×同 poly**，仅 **Stage1 Split + Stage2 Mmad**（无 Merge）。

| 张量 | 形状 |
|------|------|
| src | `[2,256]` int32 |
| split 后 A | `[4,256]` int8 |
| dst / golden | `[4,256]` int32（`A @ M0`） |
| M4.bin | 与 limb6 D 相同 |

| 模式 | 状态 |
|------|------|
| CPU  | ✓ `max_abs_diff=0` |
| SIM  | ✓ `max_abs_diff=0`（Model ~2.5s） |

```bash
bash run.sh -r cpu -v Ascend910B4
```
