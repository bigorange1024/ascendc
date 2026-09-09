# FEEDBACK-E19

| 字段 | 值 |
|------|----|
| task_id | E19 |
| verdict | **PASS** |
| wall_clock_min | **~12**（含迭代排错；末次干净 SIM ~70s；deadline 35） |
| directory | `graph_tests/toys/toy-e19-early-entry-trace/` |
| hypothesis | `D-exp-e19` / EARLY 入口标（相对 H-E3 / H-E1） |
| rounds | **8**（`TOY_ROUNDS` 默认） |
| kernel_wall_sec | **62.841**（budget 600；rc=0 非 124） |

## 结果摘要

1. **默认 8 轮**：`cd graph_tests/toys/toy-e19-early-entry-trace && SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` → **全绿**。
   - 每轮 `[e19-trace] stages set=2/16 : 0 15`
   - Host `100`×8、`111`×8；magic `E19TOY01`+`0xE9`；用例根无 stray dump
2. **入口槽语义**：见同目录 `TRACE.md`（槽 15=AIV0 极早直写；槽 0=AIC 入口 + SIM 下 AIV0 桥写）。
3. **SIM 发现**：AIC→GM（标量或 DataCopy）对 Host D2H **不可见**；AIV→GM 可见。故用 `Set(1)`/`Wait(1)` 让 AIV0 桥写槽 0，否则无法在 SIM 上验收「AIC 已过入口」。

## 对图谱节点的 effect

| node | effect | note |
|------|--------|------|
| `D-exp-e19` | **support** | stub 证明入口标可被 Host 轮询/核后 D2H 看到（SIM×8） |
| EARLY **H-E3** | **weaken（相对「管道假阴性」）** | 管道可信前提下，入口标协议可活；日后 NPU 若入口标也无 → 支撑 H-E3 |
| EARLY **H-E1** | **cite / 预备** | 日后 NPU：入口标有、业务 Mark15 无 → 聚焦 Prefix（H-E1）；本刀仅离线 stub |

一句话：E19 **support** `D-exp-e19`；与 EARLY 关系——钉住「极早标可被 Host 看见」，供上机区分 **H-E3**（连入口都无）与 **H-E1**（入口有、Prefix 后 Mark15 无）。

## 日志 / 产物路径

| 跑次 | 路径 |
|------|------|
| SIM 默认 8 轮 | `/opt/cursor/artifacts/e19-default-sim.log` |
| 用例 tee | `graph_tests/toys/toy-e19-early-entry-trace/output/host_trace.log` |
| 文档 | 同目录 `TRACE.md`、`STATUS.md` |

## 范围合规

- 仅改白名单目录 + 本 FEEDBACK outbox；未改 Encaps/stable/图谱 yaml；未 commit/push；未上机；未并行 SIM；未抄 Encrypt 业务。
- 工程壳仿 E17；kernel 自写 stub（入口标 + flag1 桥 + SET4）。
- API：CrossCore/PipeBarrier/DataCopy/标量 GM 写均有仓内既有查阅记录；白名单禁止改索引，本刀未写回。
