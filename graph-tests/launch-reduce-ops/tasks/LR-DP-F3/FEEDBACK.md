# FEEDBACK — LR-DP-F3（Decaps →2）

| 项 | 值 |
|----|-----|
| 目录 | `graph-tests/enc_related/RB-T30-decaps-2launch` |
| Host launch | **2**（`t30_dec_fused` + `t30_enc_fused`） |
| 状态 | **PASS_NPU×30**（2026-09-10） |
| 约束 | **不修改** T28/T29/D08/D09；独立重写 |
| 证据 | `/mnt/workspace/launch-reduce-logs/t30-x30-20260910-153535.log` |

## 要点

- L1 Decrypt：prep→SyncAll→NTT+dot→INTT+extract
- L2 ReEncrypt：prep(G/CBD/Â…)→SyncAll→NTT 路径（1/3+GATE4）→pack；Host FO
- 冒烟墙钟 ≈2.25s；×30 复用 fixture，ok=30

## sync_audit

无红线；SYNC-05×2（同族遗留）；SYNC-12 SyncAll×2 顶层合法。
