# TASK-E19b — 加固 TRACE 写回 + 复跑 NPU

**deadline_min**: 40  
**前置**：FEEDBACK-NPU-E19 — r0–r2 槽 0+15 可见；r3 Host 丢槽 15（设备仍 504）；非 TIMEOUT。

## 假说

AIV0 **标量** `trace[slot]=1` 在 NPU 多轮后对 Host D2H **不可见/不稳定**；槽 0（Wait 后写）仍可见。需改为 **UB→DataCopy→GM**（或等价保证可见）再验收。

## 做（subagent）

1. **只改** `graph_tests/toys/toy-e19-early-entry-trace/`：`TraceSlotStore` 改为 LocalTensor 填 1 + `DataCopy` 到 GM（写详细中文注释）。可保留标量作对照宏，默认 DataCopy。
2. 本地 **SIM** 仍须绿（`TOY_ROUNDS=8`）。
3. **远程 NPU**（同 cannlab-npu）：`TOY_ROUNDS=12`，日志 `/mnt/workspace/npu_e19b_*`，scp → `/opt/cursor/artifacts/`。
4. FEEDBACK：`graph_tests/_outbox/FEEDBACK-NPU-E19b.md`

## 判读

| 结果 | 含义 |
|------|------|
| 12 轮 0+15 全绿 | support：空 TRACE 可能含 **观测管道** 因子；Encaps FusedTraceMark 同形标量须警惕 |
| 仍第 N 轮丢 15 | 非纯 DataCopy 问题；更深 H-E4 |
| TIMEOUT 粘性挂 | 升级为真挂，对照 Encaps |

## 禁止

改 Encaps/stable；并行第二路 NPU；commit/push（父统一推）。跑完提醒关机。
