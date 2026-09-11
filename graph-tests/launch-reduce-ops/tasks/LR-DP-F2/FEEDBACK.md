# FEEDBACK — LR-DP-F2（Decaps →3）

| 项 | 值 |
|----|-----|
| 目录 | `graph-tests/enc_related/RB-T29-decaps-3launch` |
| Host launch | **3**（`dec_decrypt` 融合 prep+NTT+INTT → `enc_prep` → `enc_compute`） |
| 状态 | **PASS_NPU×30**（2026-09-10） |
| 证据 | `/mnt/workspace/launch-reduce-logs/t29-x30-20260910-052355.log` |

## 要点

- AIV0 前缀 `PrepUnpackDecrypt`（T25 NPU 安全路径）→ 全核 `SyncAll`（Wait 环外，sync_audit SYNC-12 信息级）→ 复用 flag 1/3 两轮 NTT/INTT
- 未采用：独立 prep launch、flag 5/7、SoftSync、Wait 环内 SyncAll
- DC-F2 已另建独立 `RB-D09-decrypt-1launch`（2026-09-10 **PASS_NPU×30**）；本刀 L1 仍可作旁证

## sync_audit

无红线；SYNC-05/09 与 T28 同型遗留；SyncAll 记为合法顶层用法。
