# STATUS — pass-fix-f203-compress-decompress-du10-dv4-k2/compress

**前缀 `pass-fix-`**：ML-KEM-512 W0/B1 **Compress_d** 单 poly 向量子探针；顶层默认验收 **d∈{4,10}**。

**指南**：[`docs/notes/F203-Compress-Decompress-向量实现指南.md`](../../docs/notes/F203-Compress-Decompress-向量实现指南.md)  
**ByteEncode/Decode 宏分层**：[`docs/notes/F203-ByteEncode-ByteDecode-d-向量与标量选型.md`](../../docs/notes/F203-ByteEncode-ByteDecode-d-向量与标量选型.md)

## 向量路径

| d | 路径 | CPU | SIM tick |
|---|------|-----|----------|
| 4 | int32 Barrett | PASS | **3156** |
| 5 | int32 Barrett | 本轮未验 | 本轮非默认 |
| 10 | cast_div 商 | PASS | **3442** |
| 11 | cast_div 商 | 本轮未验 | 本轮非默认 |

## 环境变量

| 变量 | 默认 | 含义 |
|------|------|------|
| `F203_COMPRESS_D` | `4` | **4 / 5 / 10 / 11** |
| `COMPRESS_D_VEC` | `1` | 0=标量 fallback / **1=向量（默认，验收基线；tail 抄此路径）** |

**说明**：Compress 为纯 per-lane 运算（Barrett / cast_div + mask），**默认激活向量**；标量路径仅作对照 fallback。

## 衔接

→ 后续 W0/B2 `pass-fix-f203-byteencode-decode-d-k2/`（comp 域 → ByteEncode；本 B1 只验 per-lane 压缩域）
