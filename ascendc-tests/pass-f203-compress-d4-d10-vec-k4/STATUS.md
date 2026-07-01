# STATUS — pass-f203-compress-d4-d10-vec-k4

**前缀 `pass-`**：本目录 **d=4 / d=10** 单用例 CPU+SIM 均已验收，可作为 Compress 向量/标量集成的**参考积木**。

**验收 d 集合**：**`d ∈ {4, 10}`**（FIPS Table 2 中 ML-KEM-512/768 档 `d_v=4`、`d_u=10`）。**不含** ml_kem_1024 的 **d=5 / d=11** —— 后者见 [`fix-f203-alg14-pke-encrypt-correctness-k4`](../fix-f203-alg14-pke-encrypt-correctness-k4/) `pack/`（已与 liboqs 对齐）。

## 目标

FIPS 203 §4.2.1 **`Compress_d`**（Eq 4.7）；Encrypt Alg.14 压缩半部（单 poly）。

## 语义与 liboqs

| d | Barrett 舍入 | 与 liboqs ref |
|---|--------------|---------------|
| 4 | `(u·1290160 + (1<<27)) >> 28` | **0 差异**（全 u∈[0,q)） |
| 10 | u64 `(u·2642263040 + (1<<32)) >> 33` | **0 差异** |

> **注意**：d=5 的正确偏置为 `(1<<26)`（非 `(1<<27)`），见 [`docs/notes/F203-PKE-liboqs交叉验证与Compress定点技术总结.md`](../../docs/notes/F203-PKE-liboqs交叉验证与Compress定点技术总结.md)。勿将本探针 d=4 公式类推至 d=5。

## 性能（910B4）

| d | 路径 | CPU | SIM totalTick |
|---|------|-----|---------------|
| 4 | 向量 Barrett | PASS | **3247** |
| 10 | 标量 u64 | PASS | **10713** |

## 环境变量

| 变量 | 默认 | 含义 |
|------|------|------|
| `F203_COMPRESS_D` | `4` | **4** 或 **10** |
| `COMPRESS_D_VEC` | `1` | d=4 向量；d=10 暂标量 |

## 衔接

→ [`pass-f203-byteencode-d4-d10-vec-k4`](../pass-f203-byteencode-d4-d10-vec-k4/)（Compress 输出 → ByteEncode）
