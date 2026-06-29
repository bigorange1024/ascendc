# frozen-int8-matmul-cube-128x512x512 — **路线废弃中止**

| | |
|--|--|
| **状态** | **废弃冻结**（2026-06-11） |
| **CPU** | ✓（1/4/8/16 AIC，`max_abs_diff=0`） |
| **SIM** | ✗ |
| **原因** | `Matmul<>` 多核 tiling 扫参；随 NTT Matmul 路线一并归档 |

文档：[qa/2026-06/2026-06-11-…#exp-int8-matmul](../../../qa/2026-06/2026-06-11-ascendc-engineering-notes与数据搬运.md#exp-int8-matmul-多核-tiling-实验)

勿 fork。见 [frozen/INDEX.md](../INDEX.md)。
