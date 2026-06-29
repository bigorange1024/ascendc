# STATUS — fix-f203-byteencode-d-vec-k4

**阶段**：A0 完成（d=4/10 CPU+SIM PASS）  
**向量路径**：256-wide `mask_low_bits_i32` + 分组标量 pack（mlk 布局）

## 目标

FIPS 203 Algorithm 5 **`ByteEncode_d`** 独立探针；Encrypt Alg.14 打包半部（d=4/10）。

## 与 byteencode12 关系

- **不修改** [`pass-fix-f203-2s1e-byteencode12-vec-k4`](../pass-fix-f203-2s1e-byteencode12-vec-k4/)（d=12 KeyGen 已 PASS）
- 本目录专责 Encrypt 路径 d=4 / d=10

## 性能（910B4，BYTE_ENCODE_D_VEC=1）

| d | out bytes | CPU | SIM totalTick | wall_sec (sim) |
|---|-----------|-----|---------------|----------------|
| 4 | 128 | PASS | **5435** | ~5.6 |
| 10 | 320 | PASS | **6455** | ~6.8 |

## 环境变量

| 变量 | 默认 | 含义 |
|------|------|------|
| `F203_BYTE_ENCODE_D` | `4` | 4（c₂）或 10（c₁） |
| `BYTE_ENCODE_D_VEC` | `1` | 0=全标量 pack；1=向量 mask |

## 下一步

- round-trip 与 [`fix-f203-alg6-bytedecode-d-vec-k4`](../fix-f203-alg6-bytedecode-d-vec-k4/)
- 链接 [`fix-f203-compress-d-vec-k4`](../fix-f203-compress-d-vec-k4/) 做 Compress+Encode 链
