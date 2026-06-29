# frozen-int8-matmul-cube-16x256x512 — **路线废弃中止**

| 字段 | 内容 |
|------|------|
| **状态** | **废弃冻结**（2026-06-11） |
| **形状** | `C[16,512]=A[16,256]×B[256,512]`（无垫片） |
| **CPU** | ✓ `max_abs_diff=0` |
| **SIM** | ✓ **孤立工程** ~9s；编入 s12-matmul Host 后仍挂 |
| **原因** | Kyber NTT Stage2 `Matmul<>` 探针；路线已废弃 |

扫参依据见同目录 `../frozen-int8-matmul-cube-128x512x512/DIM_SWEEP.md`。

勿 fork。见 [frozen/INDEX.md](../INDEX.md)。
