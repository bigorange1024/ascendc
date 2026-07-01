# STATUS — pass-f203-decompress-d4-d10-vec-k4

**前缀 `pass-`**：本目录 **d=4 / d=10** 单用例 CPU+SIM 均已验收。

**验收 d 集合**：**`d ∈ {4, 10}`**（FIPS §4.2.1 **`Decompress_d`**，Eq 4.8）。**不含** ml_kem_1024 的 **d=5 / d=11**。

## 目标

Decrypt Alg.15 解压缩半部（单 poly；输入为 d-bit 压缩域系数）。

## 性能（910B4，DECOMPRESS_D_VEC=1）

| d | CPU | SIM totalTick |
|---|-----|---------------|
| 4 | PASS | **3177** |
| 10 | PASS | **3146** |

## 环境变量

| 变量 | 默认 | 含义 |
|------|------|------|
| `F203_DECOMPRESS_D` | `4` | **4** 或 **10** |
| `DECOMPRESS_D_VEC` | `1` | d=4/10 均向量 |

## 衔接

← [`pass-f203-alg6-bytedecode-d4-d10-vec-k4`](../pass-f203-alg6-bytedecode-d4-d10-vec-k4/)（Decode → Decompress 链）
