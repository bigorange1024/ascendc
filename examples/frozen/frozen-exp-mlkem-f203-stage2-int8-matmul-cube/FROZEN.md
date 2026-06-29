# ⛔ 已冻结（2026-06-11）

**原路径**：`examples/incubating/exp-mlkem-f203-stage2-int8-matmul-cube/`

## 冻结原因

| 类别 | 说明 |
|------|------|
| **路线废弃** | 纯 AIC `Matmul<>` 做 F203 Stage2；与 limb6 批 NTT 的 `AicMmad` **GM 契约不同** |
| **未闭环** | 仅 CPU 对拍，未纳入 SIM 验收 |
| **已被取代** | 全链路拍板 `AicMmad` + merged_kyber FSM；多核扫参在 `ascendc-tests/frozen/frozen-int8-matmul-cube-*`（同属废弃 Matmul 路线） |

## 继任

- `examples/incubating/exp-sepolyvec8-ntt-k8/`（交错批 NTT）
- `ascendc-tests/frozen/frozen-fix-f203-2s1e-alg13-16171820-k4`（F203 Tag5T）

**禁止** fork、禁止把本 exp 作 Stage2 `Matmul<>` 模板。
