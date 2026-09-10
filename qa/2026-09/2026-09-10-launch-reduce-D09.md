# 2026-09-10 · Launch 压缩 · Decrypt→1（RB-D09）

## 决策

- 用户要求：**重写实验代码，勿改已有树** → 新建 `RB-D09-decrypt-1launch`，不动 D08/T28/T29。
- Compress₁ 对齐 liboqs 常数路径（非学校式 (2x+q/2)/q）。
- SyncAll 仅在 CrossCore Wait 环外；flag∈{1,3}。

## 结果

- sync_audit：无红线（SYNC-05 遗留同族）。
- NPU 冒烟 PASS；NPU×30 **ok=30 fail=0**（每轮换 SEED_D）。
- QUEUE 1–7 全绿；Decrypt launch 数对齐 stable=1。

## 遗留

- Wave4 经验入库（KB 图改须用户授权）。
