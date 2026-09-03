# graph_tests — 索引

## 双线

| 线 | 图谱 | 状态 |
|----|------|------|
| Encaps 粘性 | `docs/rg-kem-encrypt-hang.yaml` | Hostμ SIM 绿；**clean P0 绿**；等 NPU；P1 候选 |
| Decaps K | `docs/rg-kem-decrypt-k131.yaml` | **TASK-008 SIM 仍绿**；下一刀 **用户 NPU A/B/C** |

## 设计

- [`ENCRYPT_CLEAN_REWRITE.md`](ENCRYPT_CLEAN_REWRITE.md)
- [`DECRYPT_K131_PLAN.md`](DECRYPT_K131_PLAN.md)

## 工单

| 工单 | 状态 |
|------|------|
| TASK-001..006 | 闭环 |
| TASK-007 | **PASS**（clean P0）；FB 已交 |
| TASK-008 | **PASS**（Decaps SIM 基线）；FB 已交；转用户 NPU |
