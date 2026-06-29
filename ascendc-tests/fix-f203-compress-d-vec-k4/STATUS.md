# STATUS — fix-f203-compress-d-vec-k4

**阶段**：A0 完成（d=4/10 CPU+SIM PASS）  
**向量路径**：d=4 256-wide Barrett；d=10 设备标量 u64 Barrett

## 目标

FIPS 203 §4.2.1 **`Compress_d`**（Eq 4.7）；Encrypt Alg.14 前半部。

## 性能（910B4）

| d | 路径 | CPU | SIM totalTick |
|---|------|-----|---------------|
| 4 | 向量 Barrett | PASS | **3247** |
| 10 | 标量 u64 | PASS | **10713** |

## 环境变量

| 变量 | 默认 | 含义 |
|------|------|------|
| `F203_COMPRESS_D` | `4` | 4 或 10 |
| `COMPRESS_D_VEC` | `1` | d=4 向量；d=10 暂标量 |

## 衔接

→ [`fix-f203-byteencode-d-vec-k4`](../fix-f203-byteencode-d-vec-k4/)
