# STATUS — RB-D06-decaps-fo

| 字段 | 值 |
|------|-----|
| 刀 | DRW-K03 · E-K03-FO / **G-DG6-FO** / **Q-DECAPS-CORRECT** |
| 状态 | **PASS_CPU + PASS_NPU** |
| 日期 | 2026-09-09 |
| 墙钟 | NPU legit 3.254s + reject 2.459s（预算 180） |

## 验收

| 项 | 结果 |
|----|------|
| CPU | 合法+拒绝 max=0；`K_rej≠K_legit`；oracle=liboqs_kem_ref |
| NPU Ascend910B3 | 同上 |
| sync_audit | clean（SYNC-09） |

运营：[`../../decrypt-rebuild-ops/tasks/DRW-K03-decaps-fo/FEEDBACK.md`](../../decrypt-rebuild-ops/tasks/DRW-K03-decaps-fo/FEEDBACK.md)
