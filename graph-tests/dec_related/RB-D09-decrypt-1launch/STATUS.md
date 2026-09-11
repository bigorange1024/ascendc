# RB-D09-decrypt-1launch · STATUS

| 项 | 值 |
|----|-----|
| 刀 | LR-DC-F2（Decrypt →1） |
| Kernel | `d09_decrypt_fused_custom` |
| Host launch | **1** |
| Magic out | `0x44303931` ("D091") |
| Flag | ∈{1,3}；禁 SoftSync；SyncAll 仅 prep 后（Wait 环外） |
| 约束 | **独立新树**；不改 D08/T28/T29 |
| 状态 | **PASS_NPU×30**（2026-09-10） |

## Host 路径

单 launch：`ACLRT_LAUNCH_KERNEL(d09_decrypt_fused_custom)(…)` → Sync → D2H m/TRACE/out。

## 设备流

AIV0 PrepUnpack → ALL SyncAll → AIC/AIV 两轮 flag1/3（NTT+dot / INTT+extract）→ MAGIC。

## 验收

| 档 | 结果 | 证据 |
|----|------|------|
| sync_audit | 无红线（SYNC-05 遗留 + SYNC-09 + SYNC-12） | `launch-reduce-ops/tasks/LR-DC-F2/logs/sync_audit.json` |
| NPU 冒烟 | **PASS** | `/mnt/workspace/launch-reduce-logs/d09-smoke-20260910-143857.log` |
| NPU×30 | **PASS ok=30 fail=0**（每轮换 SEED_D） | `/mnt/workspace/launch-reduce-logs/d09-x30-20260910-144039.log` |

## 性能（NPU · 设备真值）

登记见 [`qa/active_npu_perf_summary.md`](../../../qa/active_npu_perf_summary.md)。

| 项 | 值 |
|----|-----|
| Σ Task Duration | **455.74 µs**（ops-profiling）/ **454.58 µs**（msopprof） |
| `d09_decrypt_fused_custom` | 单 launch |
| Freq | **1800 / 1800** MHz（满频） |
| cube0 cycles | **722923**（401.62 µs） |
| vector0 cycles | **813361**（451.87 µs；**scalar≈96.9%**） |
| vector1 cycles | **722750**（401.53 µs） |
| 采集 | ops-profiling `round_001`；msopprof Default → `docs/perf/round_002/` |
| Timeline | `TimelineDetail` dump 失败；无 PipeTimeline |