# STATUS — RB-T02-gate-timing-wait4

| 字段 | 值 |
|------|-----|
| 刀 | T02 · D-EXP-T02 |
| 状态 | **PASS**（CPU + `SIM_DIRECT=1` sim） |
| 日期 | 2026-09-08 |
| 墙钟 | ~12 min（编码+双跑+审计迭代） |

## 目标达成

1. `KERNEL_TYPE_MIX_AIC_1_2`，`blockDim=1`
2. GATE：AIC **首条** CrossCore = `Wait(4)`；AIV `Set(4)` 唤醒；随后 `Wait(1)`→Cube→`Set(3)` / AIV `Set(1)`→`Wait(3)`
3. Host 单 launch + `SynchronizeStream`；`trace_map.md` 含 flag4 与 1/3
4. CPU / SIM 均 exit 0，无 hang；用例根无 stray dump

## GATE 结论

AIC **会在** AIV `Set(4)` **之前**进入 `Wait(4)`（预期）；靠 Set 唤醒。Host 打印  
`gate: AIC entered Wait(4) before AIV Set(4) (expected; woken by Set)`。

## 验收证据

| 项 | 结果 |
|----|------|
| `bash run.sh -r cpu -v Ascend910B4` | SUCCESS；TRACE 全槽直写绿 |
| `SIM_DIRECT=1 bash run.sh -r sim …` | SUCCESS；causal+mat_c；totalTick≈5142 |
| `sync_audit --check all`（源码） | **无红线**；1× SYNC-05(高，CrossSet→LightCube 误解析假阳性)+SYNC-09 性能 |
| 用例根 stray | 无 |
| 2026-09-08 补丁 | verify：AIV TRACE 硬条件改为 AIV0\|AIV1 成对槽任一魔数（防 NPU 硬绑 AIV0 假红） |

运营回报：[`../../encrypt-rebuild-ops/tasks/T02-gate-timing-wait4/FEEDBACK.md`](../../encrypt-rebuild-ops/tasks/T02-gate-timing-wait4/FEEDBACK.md)
