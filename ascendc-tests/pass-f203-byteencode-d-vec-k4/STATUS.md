# STATUS — pass-f203-byteencode-d-vec-k4

**曾用目录名**：`pass-f203-byteencode-d4-d10-vec-k4`（2026-07-08 更名为 `*-d-vec-k4`，与 compress/decompress 探针对齐）。

**前缀 `pass-`**：本目录 **d=4/5/10/11** 单用例 CPU+SIM 均已验收。

**验收 d 集合**：**`d ∈ {4, 5, 10, 11}`**（FIPS 203 Alg.5 **`ByteEncode_d`**；输入为已压缩的 d-bit 系数）。

| d | 典型用途 | out bytes |
|---|----------|-----------|
| 4 | ML-KEM-512/768 c₂ | 128 |
| 5 | ML-KEM-1024 c₂ | 160 |
| 10 | ML-KEM-512/768 c₁ | 320 |
| 11 | ML-KEM-1024 c₁ | 352 |

## 实现要点

- **d=4/10**：8 系数/4 系数分组 + 向量 `mask_low_bits` + 标量 pack（与既有路线一致）。
- **d=5/11**：**8 系数/组** + 向量 mask + 分组标量 pack（公式对齐 ml-kem-native `poly_compress_d5/d11` 比特布局，与 Alg.5 比特流 0-diff）。
- **非**纯向量比特流：pack 环仍为标量 `SetValue`，但循环从 O(N×d) 降至 O(N/8)。

## 性能（910B4，BYTE_ENCODE_D_VEC=1）

| d | out bytes | CPU | SIM totalTick |
|---|-----------|-----|---------------|
| 4 | 128 | PASS | **5435** |
| 5 | 160 | PASS | **5537** |
| 10 | 320 | PASS | **6455** |
| 11 | 352 | PASS | **6568** |

## 环境变量

| 变量 | 默认 | 含义 |
|------|------|------|
| `F203_BYTE_ENCODE_D` | `4` | **4 / 5 / 10 / 11** |
| `BYTE_ENCODE_D_VEC` | `1` | 向量 mask + 分组标量 pack |

## 衔接

- round-trip：[`pass-f203-alg6-bytedecode-d-vec-k4`](../pass-f203-alg6-bytedecode-d-vec-k4/)
- 上游 Compress：[`pass-f203-compress-d-vec-k4`](../pass-f203-compress-d-vec-k4/)
