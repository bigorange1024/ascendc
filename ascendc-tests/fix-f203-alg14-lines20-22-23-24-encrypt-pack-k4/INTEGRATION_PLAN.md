# INTEGRATION_PLAN — fix-f203-alg14-lines20-22-23-24-encrypt-pack-k4

## 范围

| Alg.14 行 | 内容 | 本探针 |
|-----------|------|--------|
| 20 | m → μ_embed（**输出对拍，不加 v**） | ✓ |
| 21 | v + μ + e₂ | ✗（compute 探针） |
| 22–23 | Compress₁₁(u) + ByteEncode → c₁ | ✓ |
| 24 | Compress₅(v) + ByteEncode → c₂ | ✓ |

## GM 契约（与 `pass-fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4` 对齐）

| 指针 | 形状 / 大小 | 说明 |
|------|-------------|------|
| `mGm` | uint8[32] | 明文消息 |
| `uGm` | int32[4,256] 行主序 | compute `uOut`（无 μ） |
| `vGm` | int32[256] | compute `vOut`（**无 μ**；含 e₂ 由 compute 负责） |
| `muEmbedGm` | int32[256] | 行 20 输出；`mu[c] ∈ {0,1665}` |
| `cGm` | uint8[1568] | c₁[1408] ‖ c₂[160] |

## 设备编排

- 单 launch `f203_encrypt_alg14_tail`
- `KERNEL_TYPE_AIV_ONLY`，`blockDim=1`
- 顺序：μ_embed → 4×(Compress₁₁+ByteEncode₁₁) → Compress₅+ByteEncode₅

## 与 compute 合并（日后抄码，禁止跨探针引用）

插入点：`f203_encrypt_l18_l19_kernel.cpp` 在 `AIV_V_DONE`（v 含 e₂）之后：

1. 从 GM 读 `m`，写 `mu_embed`（或内联不写 GM）
2. **行 21**：`v += mu_embed`（compute 已有逻辑时合并）
3. 本探针 `compute/f203_tail_compress_byteencode.hpp` 抄入 pack 段（**禁止** `#include` 其它探针）

## 验收

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

对拍：`golden/mu_embed.bin`、`golden/c.bin`。

## 实现注记

- Compress₅：int32 Barrett 向量（抄 `pass-f203-compress-d-vec-k4`）
- Compress₁₁：cast_div 商向量（同上）
- ByteEncode：8 系数/组 pack（抄 `pass-f203-byteencode-d-vec-k4`；替代原 Alg.5 比特流标量）
