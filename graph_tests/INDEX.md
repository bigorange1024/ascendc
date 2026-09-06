# graph_tests — 索引

## 主线（2026-09-06）

| 项 | 路径 | 状态 |
|----|------|------|
| **Encrypt 实机无卡死** | 知识库 `docs/notes/Encrypt-实机无卡死-知识库.md` · 图谱 `docs/rg-encrypt-npu-hangfree.yaml` · [`ENCRYPT_REWRITE_PLAN.md`](ENCRYPT_REWRITE_PLAN.md) | **库+图已建**；实验等下令 |
| toys / enc_related | [`toys/`](toys/INDEX.md) · [`enc_related/`](enc_related/INDEX.md) | E01–E07 toys 已跑通（INTT 同系 ≠Tag5T） |

## 旧三线（只读参考，禁止混进新主线）

| 线 | 图谱 | 状态 |
|----|------|------|
| Encaps 粘性 | `docs/rg-kem-encrypt-hang.yaml` | 冻结参考 |
| PKE Decrypt 卡死 | `docs/rg-kem-decrypt-hang.yaml` | 冻结参考 |
| Decaps K | `docs/rg-kem-decrypt-k131.yaml` | 与 hang 正交 |

## 设计

- [`ENCRYPT_REWRITE_PLAN.md`](ENCRYPT_REWRITE_PLAN.md)（**当前计划真理源**）
- [`ENCRYPT_CLEAN_REWRITE.md`](ENCRYPT_CLEAN_REWRITE.md)（旧加法线，冻结）
- [`DECRYPT_HANG_PLAN.md`](DECRYPT_HANG_PLAN.md)
- [`DECRYPT_K131_PLAN.md`](DECRYPT_K131_PLAN.md)

## 旧工单（冻结参考）

| 工单 | 状态 |
|------|------|
| TASK-001..012 | 旧 hang/clean 线记录；**不再续改代码** |
