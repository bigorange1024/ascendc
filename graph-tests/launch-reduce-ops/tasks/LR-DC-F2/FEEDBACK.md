# FEEDBACK — LR-DC-F2（Decrypt →1）

| 项 | 值 |
|----|-----|
| 目录 | `graph-tests/dec_related/RB-D09-decrypt-1launch` |
| Host launch | **1**（`d09_decrypt_fused_custom`：prep+NTT+INTT） |
| 状态 | **IN_PROGRESS**（新建树；待 NPU 冒烟 / ×30） |
| 约束 | **不修改** D08 / T28 / T29 既有源码 |

## 要点

- 独立 basename / namespace（`d09` / `rb_d09`）；与 T29 L1 证据解耦
- SyncAll 在 Wait 环外；flag∈{1,3}；禁 SoftSync / flag 5·7
- Compress₁ 对齐 liboqs 常数路径

## sync_audit / NPU

见 `logs/`（跑完后填）。
