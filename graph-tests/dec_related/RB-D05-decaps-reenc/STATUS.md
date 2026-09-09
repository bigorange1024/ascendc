# STATUS — RB-D05-decaps-reenc

| 字段 | 值 |
|------|-----|
| 刀 | DRW-K02 · E-K02-REENC / **G-DG5-REENC** |
| 状态 | **PASS_CPU + PASS_NPU** |
| 日期 | 2026-09-09 |
| 墙钟 | CPU kernel≈5.15s；NPU `wall_sec=3.255`（预算 180） |

## 目标达成

1. 双 launch：`reenc_prep_custom` → mid-sync → `reenc_mix_custom`（MIX 1/3+GATE4）
2. `BLOCK_DIM=1`；禁 flag 5/7；X12 DataCopy
3. I/O：`ek+m'+r' → c'[1568]`；Host 不预喂最终 c'

## 验收证据

| 项 | 结果 |
|----|------|
| CPU Ascend910B4 | c'≡liboqs_pke_ref max=0；diag max=0 |
| NPU Ascend910B3（并行上板） | 同上；`wall_sec=3.255` |
| sync_audit | clean（SYNC-05+SYNC-09） |

运营：[`../../decrypt-rebuild-ops/tasks/DRW-K02-decaps-reenc/FEEDBACK.md`](../../decrypt-rebuild-ops/tasks/DRW-K02-decaps-reenc/FEEDBACK.md)
