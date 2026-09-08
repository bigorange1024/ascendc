# RB-T06 密文布局 — c₁‖c₂（ML-KEM-1024）

| 段 | 内容 | 字节偏移 | 长度 |
|----|------|----------|------|
| **c₁** | `ByteEncode₁₁(Compress₁₁(u))`，k=4 poly，每 poly 352B | `[0, 1408)` | 1408 |
| **c₂** | `ByteEncode₅(Compress₅(v))`，1 poly | `[1408, 1568)` | 160 |
| **c** | `c₁ ‖ c₂` | `[0, 1568)` | **1568** |

输入：`u[1024] int32`、`v[256] int32`（canonical mod q=3329）。  
验收：`output/c.bin` 与 `output/golden_c.bin` 逐字节一致。
