# STATUS — pass-f203-alg6-bytedecode-d4-d10-vec-k4

**前缀 `pass-`**：本目录 **d=4 / d=10** 单用例 CPU+SIM 均已验收。

**验收 d 集合**：**`d ∈ {4, 10}`**（FIPS Alg.6 **`ByteDecode_d`**；输出 d-bit 整数，**不含** Decompress）。**不含** ml_kem_1024 的 **d=5 / d=11** —— 后者见 [`fix-f203-alg15-pke-decrypt-correctness-k4`](../fix-f203-alg15-pke-decrypt-correctness-k4/) `unpack/`。

## 目标

Decrypt 侧密文解包半部（单 poly）。

## 配对探针

- [`pass-f203-byteencode-d4-d10-vec-k4`](../pass-f203-byteencode-d4-d10-vec-k4/) round-trip golden
- 下游 Decompress：[`pass-f203-decompress-d4-d10-vec-k4`](../pass-f203-decompress-d4-d10-vec-k4/)
- **d=12** KeyGen：[`pass-fix-f203-2s1e-byteencode12-vec-k4`](../pass-fix-f203-2s1e-byteencode12-vec-k4/)

## 性能（910B4，BYTE_DECODE_D_VEC=1）

| d | in bytes | CPU | SIM totalTick |
|---|----------|-----|---------------|
| 4 | 128 | PASS | **9186** |
| 10 | 320 | PASS | **6546** |

## 环境变量

| 变量 | 默认 | 含义 |
|------|------|------|
| `F203_BYTE_DECODE_D` | `4` | **4** 或 **10** |
| `BYTE_DECODE_D_VEC` | `1` | d=4 向量 nibble mask |
