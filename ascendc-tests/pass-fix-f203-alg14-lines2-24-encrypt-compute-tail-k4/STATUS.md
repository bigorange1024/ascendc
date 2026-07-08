# STATUS — pass-fix-f203-alg14-lines2-24-encrypt-compute-tail-k4

**前缀 `pass-`**：Alg.14 **行 2、16–24**（prep 行 3–15 外）→ 密文 **c = c₁ ‖ c₂**（1568B，ML-KEM-1024）；**CPU+SIM 验收 PASS**（2026-07-08）。

**定位**：完整 Encrypt 的 **compute + pack 基线**；下一步在此用例上接 prep（行 3–15）形成全链 Encrypt。

## Alg.14 覆盖（prep 素材由 host 提供）

| 行 | 内容 | 设备 |
|----|------|------|
| 2 | `t̂ ← ByteDecode₁₂(ek)` | SIM ✓ |
| 16–17 | `ŷ ← NTT(y)` | ✓ |
| 18 | `û, tr̂ ← (Âᵀ\|t̂ᵀ) ∘ ŷ` | ✓ |
| 19 | `u ← INTT(û) + e₁` | ✓ |
| 20–21 | `e₂+=μ`；`v ← INTT(tr̂)+e₂'` | ✓ |
| 22–24 | `c₁‖c₂ ← Compress+ByteEncode(u,v)` | ✓ |

**不含**：行 3–15（ρ→Â、CBD→y/e₁/e₂）→ 全链见 [`pass-fix-f203-alg14-pke-encrypt-device-k4`](../pass-fix-f203-alg14-pke-encrypt-device-k4/)（方案已定）。

## Launch 模式

| 模式 | launch 数 | 说明 |
|------|-----------|------|
| **SIM** | **1** | `f203_encrypt_l18_l19`：compute + 双 AIV 分片内联 tail pack → `u,v,c` |
| **CPU** | **4** | 三 launch compute 产 `u`；`v`=golden；独立 `f203_encrypt_alg14_pack` 产 `c` |

## 输出布局（c = c₁ ‖ c₂）

| 段 | 大小 | 含义 |
|----|------|------|
| c₁ | 4×352 = **1408 B** | `ByteEncode₁₁(Compress₁₁(u[p]))` |
| c₂ | **160 B** | `ByteEncode₅(Compress₅(v))` |
| **c** | **1568 B** | 标准 ML-KEM-1024 密文 |

## 验收（910B4）

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

| 模式 | c / u / v | SIM tick |
|------|-----------|----------|
| CPU | max=0（v=golden） | ~27s |
| SIM | max=0（全设备） | **154781** |

## 文档

- 集成设计：[`INTEGRATION_PLAN.md`](INTEGRATION_PLAN.md)
- GM 契约：[`f203_encrypt_compute_tail_layout.h`](f203_encrypt_compute_tail_layout.h)
- tail 选型：[`docs/notes/F203-ByteEncode-ByteDecode-d-向量与标量选型.md`](../../docs/notes/F203-ByteEncode-ByteDecode-d-向量与标量选型.md)
- 定稿总结：[`docs/notes/F203-Alg14-Encrypt-compute-tail-PASS技术总结.md`](../../docs/notes/F203-Alg14-Encrypt-compute-tail-PASS技术总结.md)

## 上游 / 后继

| 探针 | 关系 |
|------|------|
| `pass-fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4` | compute 段 vendoring 来源 |
| `pass-fix-f203-alg14-lines20-22-23-24-encrypt-pack-k4` | tail pack 抄码来源（已内联 SIM） |
| prep + 本探针 | 全链：[`pass-fix-f203-alg14-pke-encrypt-device-k4`](../pass-fix-f203-alg14-pke-encrypt-device-k4/) |
