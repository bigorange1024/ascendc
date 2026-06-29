# STATUS — fix-f203-decompress-d-vec-k4

**阶段**：A0 完成（d=4/10 CPU+SIM PASS）  
**向量路径**：256-wide `Muls(Q)+Adds(bias)+ShiftRight(d)`

## 目标

FIPS 203 §4.2.1 **`Decompress_d`**（Eq 4.8）；Decrypt Alg.15 前半部。

## 性能（910B4，DECOMPRESS_D_VEC=1）

| d | CPU | SIM totalTick |
|---|-----|---------------|
| 4 | PASS | **3177** |
| 10 | PASS | **3146** |

## 环境变量

| 变量 | 默认 | 含义 |
|------|------|------|
| `F203_DECOMPRESS_D` | `4` | 4 或 10 |
| `DECOMPRESS_D_VEC` | `1` | 向量（d=4/10 均支持） |

## 衔接

← [`fix-f203-alg6-bytedecode-d-vec-k4`](../fix-f203-alg6-bytedecode-d-vec-k4/)
