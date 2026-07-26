# STATUS — pass-fix-f203-alg14-lines20-22-23-24-encrypt-pack-k4

**前缀 `pass-`**：Alg.14 **tail 段**（行 20 μ_embed + 行 22–24 pack→`c`）单算子探针；**CPU+SIM 验收 PASS**（2026-07-08）。

**定位**：tail pack **功能验证**与抄码来源；行 20–21 的 μ 折叠与 pack 内联已合入 [`pass-fix-f203-alg14-lines2-24-encrypt-compute-tail-k4`](../pass-fix-f203-alg14-lines2-24-encrypt-compute-tail-k4/)（SIM 1 launch）。本探针保留独立验收 tail 算子组合。

## 目标

Alg.14 加密 tail：行 20 μ_embed 输出 + 行 22–24 ciphertext pack（不晋级 stable）。

## 验证

| 模式 | 状态 | 备注 |
|------|------|------|
| CPU tikicpu | ✓ | `bash run.sh -r cpu -v Ascend910B4` |
| SIM CaModel | ✓ **56259 tick** | `SIM_DIRECT=1 bash run.sh -r sim`（分组 pack；原比特流 ~227k） |

## 输出

- `output/mu_embed.bin` — int32[256]
- `output/c.bin` — uint8[1568]

## 依赖

- 输入 `m/u/v` 由 `scripts/gen_data.py` 随机生成（与 golden 同源公式）
- u/v 布局与 compute 探针 `uOut`/`vOut` 一致

## 后继

- 全链 Encrypt 集成：[`pass-fix-f203-alg14-pke-encrypt-device-k4`](../pass-fix-f203-alg14-pke-encrypt-device-k4/)
