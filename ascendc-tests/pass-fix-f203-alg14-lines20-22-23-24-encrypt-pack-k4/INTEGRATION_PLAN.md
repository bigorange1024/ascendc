# INTEGRATION_PLAN — pass-fix-f203-alg14-lines20-22-23-24-encrypt-pack-k4

**状态**：**PASS**（2026-07-08 晋级 `pass-`）；tail 单算子探针。

## 范围

| Alg.14 行 | 内容 | 本探针 |
|-----------|------|--------|
| 20 | m → μ_embed（**输出对拍，不加 v**） | ✓ |
| 21 | v + μ + e₂ | ✗（compute 探针） |
| 22–23 | Compress₁₁(u) + ByteEncode → c₁ | ✓ |
| 24 | Compress₅(v) + ByteEncode → c₂ | ✓ |

## GM 契约（与 compute 探针对齐）

| 指针 | 形状 / 大小 | 说明 |
|------|-------------|------|
| `mGm` | uint8[32] | 明文消息 |
| `uGm` | int32[4,256] 行主序 | compute `uOut` |
| `vGm` | int32[256] | compute `vOut` |
| `muEmbedGm` | int32[256] | 行 20 输出 |
| `cGm` | uint8[1568] | c₁[1408] ‖ c₂[160] |

## 设备编排

- 单 launch `f203_encrypt_alg14_tail`
- `KERNEL_TYPE_AIV_ONLY`，`blockDim=1`

## 与 compute+tail 探针关系

[`pass-fix-f203-alg14-lines2-24-encrypt-compute-tail-k4`](../pass-fix-f203-alg14-lines2-24-encrypt-compute-tail-k4/) 已内联 pack（SIM 1 launch）；本探针保留 **独立 tail launch** 供单算子回归。

## 验收

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```
