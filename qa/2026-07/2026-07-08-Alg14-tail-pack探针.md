# 2026-07-08 — Compress/ByteEncode 扩档、tail pack、内核超时口径

## 1. Compress / Decompress 探针

| 旧目录 | 新目录 |
|--------|--------|
| `pass-f203-compress-d4-d10-vec-k4` | **`pass-f203-compress-d-vec-k4`** |
| `pass-f203-decompress-d4-d10-vec-k4` | **`pass-f203-decompress-d-vec-k4`** |

- 验收 **d∈{4,5,10,11}** CPU+SIM PASS
- d=5：int32 Barrett（bias **`1<<26`**）；d=11：cast_div 商向量
- 指南：`docs/notes/F203-Compress-Decompress-向量实现指南.md`

SIM tick（compress，910B4）：d=4 **3247** · d=5 **3121** · d=10 **3449** · d=11 **3399**

## 2. ByteEncode / ByteDecode 探针

| 旧目录 | 新目录 |
|--------|--------|
| `pass-f203-byteencode-d4-d10-vec-k4` | **`pass-f203-byteencode-d-vec-k4`** |
| `pass-f203-alg6-bytedecode-d4-d10-vec-k4` | **`pass-f203-alg6-bytedecode-d-vec-k4`** |

- 扩展 **d=5/11**：8 系数/组 + 向量 `mask_low_bits` + 分组 pack/unpack（与 ml-kem-native 比特布局 / Alg.5 0-diff）
- 全档 **d∈{4,5,10,11}** CPU+SIM PASS

SIM tick（910B4）：encode d=4 **5435** · d=5 **5537** · d=10 **6455** · d=11 **6568**；decode d=4 **9186** · d=5 **5696** · d=10 **6546** · d=11 **6641**

## 3. Alg.14 tail pack 探针

目录：`fix-f203-alg14-lines20-22-23-24-encrypt-pack-k4`

| 行 | 内容 | 状态 |
|----|------|------|
| 20 | m → μ_embed（输出，不加 v） | ✓ |
| 22–23 | Compress₁₁(u) + ByteEncode → c₁ | ✓ |
| 24 | Compress₅(v) + ByteEncode → c₂ | ✓ |

- Compress 向量抄自 `pass-f203-compress-d-vec-k4`
- ByteEncode：**分组 pack** 抄自 `pass-f203-byteencode-d-vec-k4`（替代 Alg.5 比特流标量）
- 功能验证探针，不晋级；合并 compute 时 **抄码**、禁止跨探针 `#include`

| 模式 | 结果 |
|------|------|
| CPU | PASS（mu_embed、c max=0） |
| SIM | PASS **56259** tick（原比特流 ~227k–256k，约 **4×** 下降） |

## 4. 内核超时口径修正（全仓）

**决策**：`KERNEL_COMPUTE_BUDGET_SEC` = 各用例 `run.sh` **防挂死**预算（60s–1800s）；**~15s** 仅为 **ML-KEM Tag5T NTT 全流程** SIM **性能定标**，不适用于 KeyGen / Encrypt 全链 / Compress / tail pack 等。

| 产出 | 路径 |
|------|------|
| 定稿 | `docs/engineering/内核计算超时与性能定标.md` |
| Rule | `.cursor/rules/ascendc-development.mdc`「用例计算超时」 |
| 复现指南 | `docs/engineering/环境复现与开发指南.md` §6.5、§12–§14 |
| Skill | `ascendc-engineering-notes` §8 |

## 5. 遗留 / 下一步

| 项 | 说明 |
|----|------|
| **T17** | prep + compute + **tail** GM 级拼接（目标 2 launch Encrypt 核心） |
| tail → compute | `f203_tail_compress_byteencode.hpp` 已可抄入 compute 行 22–24 段 |
| Encrypt 全链 SIM | 仍以各段 `run.sh` 预算为准，勿用 15s 门禁 |

## 6. 证据

```bash
# byteencode d=5
F203_BYTE_ENCODE_D=5 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4  # PASS totalTick=5537

# tail pack
cd ascendc-tests/fix-f203-alg14-lines20-22-23-24-encrypt-pack-k4
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4  # PASS totalTick=56259
```
