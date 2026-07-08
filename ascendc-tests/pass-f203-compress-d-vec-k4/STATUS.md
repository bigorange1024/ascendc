# STATUS — pass-f203-compress-d-vec-k4

**前缀 `pass-`**：FIPS 203 **Compress_d** 单 poly 向量探针；**d∈{4,5,10,11}** CPU+SIM 验收。

**指南**：[`docs/notes/F203-Compress-Decompress-向量实现指南.md`](../../docs/notes/F203-Compress-Decompress-向量实现指南.md)

## 向量路径

| d | 路径 | CPU | SIM tick |
|---|------|-----|----------|
| 4 | int32 Barrett | PASS | **3247** |
| 5 | int32 Barrett | PASS | **3121** |
| 10 | cast_div 商 | PASS | **3449** |
| 11 | cast_div 商 | PASS | **3399** |

## 环境变量

| 变量 | 默认 | 含义 |
|------|------|------|
| `F203_COMPRESS_D` | `4` | **4 / 5 / 10 / 11** |
| `COMPRESS_D_VEC` | `1` | 0=标量 fallback |

## 衔接

→ [`pass-f203-byteencode-d-vec-k4`](../pass-f203-byteencode-d-vec-k4/)（comp 域 → ByteEncode；**d=4/5/10/11** PASS）
