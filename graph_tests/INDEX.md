# graph_tests — 索引

## 三线（禁止混图）

| 线 | 图谱 | 状态 |
|----|------|------|
| Encaps 粘性 | `docs/rg-kem-encrypt-hang.yaml` | Hostμ SIM 绿；**clean P0 绿**；等 NPU |
| **PKE Decrypt 卡死** | `docs/rg-kem-decrypt-hang.yaml` | **W0**；**TASK-009** 建握手 toy；**未沉机制不上机** |
| Decaps K | `docs/rg-kem-decrypt-k131.yaml` | TASK-008 SIM 仍绿；与 hang 正交 |

## 设计

- [`ENCRYPT_CLEAN_REWRITE.md`](ENCRYPT_CLEAN_REWRITE.md)
- [`DECRYPT_HANG_PLAN.md`](DECRYPT_HANG_PLAN.md)
- [`DECRYPT_K131_PLAN.md`](DECRYPT_K131_PLAN.md)

## 工单

| 工单 | 状态 |
|------|------|
| TASK-001..006 | 闭环（Encrypt hang） |
| TASK-007 | **PASS**（clean P0）；FB 已交 |
| TASK-008 | **PASS**（Decaps K SIM 基线）；与 hang 正交 |
| **TASK-009** | **进行中**：`fix-decrypt-skel-mix-chain-toy` |
