# STATUS — fix-f203-alg6-bytedecode-d-vec-k4

**阶段**：A0 完成（d=4/10 CPU+SIM PASS）  
**向量路径**：d=4 128-wide nibble mask + 交织；d=10 64 组 5B unpack（标量，mlk 布局）

## 目标

FIPS 203 Alg.6 **`ByteDecode_d`** 向量探针；Decrypt 侧 `Decompress_d(ByteDecode_d(c))` 的 decode 半部。

## 配对探针

- [`fix-f203-byteencode-d-vec-k4`](../fix-f203-byteencode-d-vec-k4/) round-trip golden
- 下游 [`fix-f203-decompress-d-vec-k4`](../fix-f203-decompress-d-vec-k4/)

## 性能（910B4，BYTE_DECODE_D_VEC=1）

| d | in bytes | CPU | SIM totalTick | wall_sec (sim) |
|---|----------|-----|---------------|----------------|
| 4 | 128 | PASS | **9186** | ~8.1 |
| 10 | 320 | PASS | **6546** | ~5.6 |

## 环境变量

| 变量 | 默认 | 含义 |
|------|------|------|
| `F203_BYTE_DECODE_D` | `4` | 4（c₂）或 10（c₁） |
| `BYTE_DECODE_D_VEC` | `1` | d=4 向量 nibble mask |
