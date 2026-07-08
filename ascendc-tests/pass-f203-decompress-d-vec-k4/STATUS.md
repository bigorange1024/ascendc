# STATUS — pass-f203-decompress-d-vec-k4

**前缀 `pass-`**：FIPS 203 **Decompress_d** 单 poly 向量探针；**d∈{4,5,10,11}** CPU+SIM 验收。

**指南**：[`docs/notes/F203-Compress-Decompress-向量实现指南.md`](../../docs/notes/F203-Compress-Decompress-向量实现指南.md)

## 向量路径

全档统一：`Muls(q)` → `Adds(2^{d-1})` → `ShiftRight(d)`（int32，无 u64）。

| d | bias | SIM tick |
|---|------|----------|
| 4 | 8 | **3177** |
| 5 | 16 | **3177** |
| 10 | 512 | **3146** |
| 11 | 1024 | **3184** |

## 环境变量

| 变量 | 默认 | 含义 |
|------|------|------|
| `F203_DECOMPRESS_D` | `4` | **4 / 5 / 10 / 11** |
| `DECOMPRESS_D_VEC` | `1` | 0=标量 fallback |

## 衔接

← [`pass-f203-alg6-bytedecode-d-vec-k4`](../pass-f203-alg6-bytedecode-d-vec-k4/)
