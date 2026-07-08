# STATUS — pass-f203-alg6-bytedecode-d-vec-k4

**曾用目录名**：`pass-f203-alg6-bytedecode-d4-d10-vec-k4`（2026-07-08 更名；验收 d 扩至 4/5/10/11）。

**前缀 `pass-`**：本目录 **d=4/5/10/11** 单用例 CPU+SIM 均已验收。

**验收 d 集合**：**`d ∈ {4, 5, 10, 11}`**（FIPS Alg.6 **`ByteDecode_d`**；输出 d-bit 整数，**不含** Decompress）。

## 目标

Decrypt 侧密文解包半部（单 poly）。

## 配对探针

- [`pass-f203-byteencode-d-vec-k4`](../pass-f203-byteencode-d-vec-k4/) round-trip golden
- 下游 Decompress：[`pass-f203-decompress-d-vec-k4`](../pass-f203-decompress-d-vec-k4/)
- **d=12** KeyGen：[`pass-fix-f203-2s1e-byteencode12-vec-k4`](../pass-fix-f203-2s1e-byteencode12-vec-k4/)

## 性能（910B4，BYTE_DECODE_D_VEC=1）

| d | in bytes | CPU | SIM totalTick |
|---|----------|-----|---------------|
| 4 | 128 | PASS | **9186** |
| 5 | 160 | PASS | **5696** |
| 10 | 320 | PASS | **6546** |
| 11 | 352 | PASS | **6641** |

## 环境变量

| 变量 | 默认 | 含义 |
|------|------|------|
| `F203_BYTE_DECODE_D` | `4` | **4 / 5 / 10 / 11** |
| `BYTE_DECODE_D_VEC` | `1` | d=4 向量 nibble mask；d=5/10/11 分组 unpack |
