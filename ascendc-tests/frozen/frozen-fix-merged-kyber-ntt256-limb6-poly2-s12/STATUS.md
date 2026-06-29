# frozen-fix-merged-kyber-ntt256-limb6-poly2-s12

> **已冻结**（2026-06-12）：merged_kyber Stage1+2 遗产；只读对照。

从 [`frozen-merged-kyber-ntt256-limb6-poly2-s12`](../frozen-merged-kyber-ntt256-limb6-poly2-s12/) 复制；**Stage1 改为单 TPipe batch Split**（无 per-poly 循环构造 `AivSplit`）。

| 张量 | 形状 |
|------|------|
| src | `[2,256]` int32 |
| split 后 A | `[4,256]` int8 |
| dst / golden | `[4,256]` int32（`A @ M0`） |

| 模式 | 状态 |
|------|------|
| CPU  | ✓ `max_abs_diff=0`，`dst.bin` md5 与 poly2-s12 一致 |

```bash
bash run.sh -r cpu -v Ascend910B4
```
