# STATUS — pass-f203-byteencode-d4-d10-vec-k4

**前缀 `pass-`**：本目录 **d=4 / d=10** 单用例 CPU+SIM 均已验收。

**验收 d 集合**：**`d ∈ {4, 10}`**（`ByteEncode_d` 比特打包；输入为已压缩的 d-bit 系数）。**不含** ml_kem_1024 的 **d=5 / d=11** —— 后者在 Encrypt 全链 `pack/f203_encrypt_pack_entry.cpp` 内联实现。

## 目标

FIPS 203 Algorithm 5 **`ByteEncode_d`**；Encrypt 侧打包半部（单 poly）。

## 与 byteencode12 关系

- **不修改** [`pass-fix-f203-2s1e-byteencode12-vec-k4`](../pass-fix-f203-2s1e-byteencode12-vec-k4/)（**d=12** KeyGen）
- 本目录专责 **d=4 / d=10**

## 性能（910B4，BYTE_ENCODE_D_VEC=1）

| d | out bytes | CPU | SIM totalTick |
|---|-----------|-----|---------------|
| 4 | 128 | PASS | **5435** |
| 10 | 320 | PASS | **6455** |

## 环境变量

| 变量 | 默认 | 含义 |
|------|------|------|
| `F203_BYTE_ENCODE_D` | `4` | **4** 或 **10** |
| `BYTE_ENCODE_D_VEC` | `1` | 向量 mask + 分组标量 pack |

## 衔接

- round-trip：[`pass-f203-alg6-bytedecode-d4-d10-vec-k4`](../pass-f203-alg6-bytedecode-d4-d10-vec-k4/)
- 上游 Compress：[`pass-f203-compress-d4-d10-vec-k4`](../pass-f203-compress-d4-d10-vec-k4/)
