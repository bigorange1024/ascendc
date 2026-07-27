# STATUS — pass-fix-f203-compress-decompress-du10-dv4-k2/decompress

**前缀 `pass-fix-`**：ML-KEM-512 W0/B1 **Decompress_d** 单 poly 向量子探针；顶层默认验收 **d∈{4,10}**。

**指南**：[`docs/notes/F203-Compress-Decompress-向量实现指南.md`](../../docs/notes/F203-Compress-Decompress-向量实现指南.md)  
**ByteEncode/Decode 宏分层**：[`docs/notes/F203-ByteEncode-ByteDecode-d-向量与标量选型.md`](../../docs/notes/F203-ByteEncode-ByteDecode-d-向量与标量选型.md)

## 向量路径

全档统一：`Muls(q)` → `Adds(2^{d-1})` → `ShiftRight(d)`（int32，无 u64）。**纯 per-lane 线性运算，默认向量优于标量**。

| d | bias | CPU | SIM tick（VEC=1） |
|---|------|-----|----------|
| 4 | 8 | PASS | **3236** |
| 5 | 16 | 本轮未验 | 本轮非默认 |
| 10 | 512 | PASS | **3317** |
| 11 | 1024 | 本轮未验 | 本轮非默认 |

## 环境变量

| 变量 | 默认 | 含义 |
|------|------|------|
| `F203_DECOMPRESS_D` | `4` | **4 / 5 / 10 / 11** |
| `DECOMPRESS_D_VEC` | `1` | 0=标量 fallback / **1=向量（默认，验收基线）** |

**与 ByteEncode 对比**：Decompress **无 bit shuffle**，与 Compress 同属 per-lane 向量友好算子；**无需**也**未做** encode 式 VEC=2 实验。Decrypt 链：`ByteDecode`（标量 unpack d=5/11）→ `Decompress`（向量）。

## 衔接

← 后续 W0/B2 `pass-fix-f203-byteencode-decode-d-k2/`
