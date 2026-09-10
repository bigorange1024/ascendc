# FEEDBACK — LR-DC-F2（Decrypt →1）

| 项 | 值 |
|----|-----|
| 目录 | `graph-tests/dec_related/RB-D09-decrypt-1launch` |
| Host launch | **1**（`d09_decrypt_fused_custom`：prep+NTT+INTT） |
| 状态 | **PASS_NPU×30**（2026-09-10） |
| 约束 | **不修改** D08 / T28 / T29 既有源码 |
| 证据 | `/mnt/workspace/launch-reduce-logs/d09-x30-20260910-144039.log` |

## 要点

- 独立 basename / namespace（`d09` / `rb_d09`）；与 T29 L1 证据解耦
- SyncAll 在 Wait 环外；flag∈{1,3}；禁 SoftSync / flag 5·7
- Compress₁ 对齐 liboqs 常数路径（`C=41285357`）
- Prep：直读 H2D dk/c；禁 ws SetValue 镜像

## NPU 冒烟（2026-09-10）

- 机：`cannlab-npu-1` / Ascend910B3 / `ASCEND_DEVICE_ID=0`
- `bash run.sh -r npu -v Ascend910B3` → `[SUCCESS] RB-D09 PASS_SYNC + PASS_IO oracle=liboqs`
- TRACE：PREP→WAIT3_NTT→NTT_DOT→WAIT3_INTT→EXTRACT；magic `0x44303931`
- 日志：`/mnt/workspace/launch-reduce-logs/d09-smoke-20260910-143857.log`

## NPU×30（2026-09-10）

- 快循环：已编译 bin + 每轮 `SEED_D=20260910…20260939` 重生 fixture
- **SUMMARY_FAST ok=30 fail=0**
- 日志：`/mnt/workspace/launch-reduce-logs/d09-x30-20260910-144039.log`

## sync_audit

无红线；SYNC-05（PIPE 方向，同 T28/T29 遗留）×1；SYNC-09 性能；SYNC-12 SyncAll 顶层合法。
