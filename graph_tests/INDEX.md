# graph_tests — 索引

## 三线（禁止混图）

| 线 | 图谱 | 状态 |
|----|------|------|
| Encaps 粘性 | `docs/rg-kem-encrypt-hang.yaml` | Hostμ SIM 绿；**clean P0 绿**；等 NPU |
| **PKE Decrypt 卡死** | `docs/rg-kem-decrypt-hang.yaml` | T0–T3 + TASK-012 脏 softSync 已沉（两档绿） |
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
| **TASK-009** | **PASS**：A magic≈3.9s；OMIT_SET4 **124** |
| **TASK-010** | **PARTIAL**：`OMIT_SLOT0` SIM **未挂**（空 while 非 SIM hang 代理） |
| **TASK-011** | **PASS**：`OMIT_SET4_R2` → **124** |
| **TASK-012** | **PASS**：Host `SKEL_SOFTSYNC_PREFILL` 0/1 双档绿 → support `J-dirty-softsync-hang-vs-race` |
