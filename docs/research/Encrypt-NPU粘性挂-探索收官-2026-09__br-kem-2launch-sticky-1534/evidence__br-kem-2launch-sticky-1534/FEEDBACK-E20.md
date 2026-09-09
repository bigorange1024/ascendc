# FEEDBACK-E20

| 字段 | 值 |
|------|----|
| task_id | E20 |
| verdict | **PASS** |
| directory | `graph_tests/toys/toy-e20-postmark-tail/` |
| hypothesis | `J-hang-after-full-trace`（全 Mark 之后 AIV-only 尾包结构） |
| rounds | **8**（`TOY_ROUNDS` 默认） |
| kernel_wall_sec | **110.181**（budget 600；rc=0 非 124） |

## 结果摘要

1. **默认 8 轮**：`cd graph_tests/toys/toy-e20-postmark-tail && SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` → **全绿**。
   - Host TRACE：`100`×8、`111`×8（末码 `111`）
   - 设备：每轮 AIC `400/401/403/410/411/418/420/421/423` 后早退（无尾包号）；AIV0 `500…523` + `540/541`；AIV1 `510…533` + `550/551`
   - magic `E20TOY01` + `0xE0` OK；用例根无 stray dump
2. **结构要点**：INTT 复用 1/3 末次 `FsmSet(3)` 后 AIC `return`；双 AIV 非对称 stub（heavy=6 / light=2）自建 `TPipe`+`DataCopy` GM↔UB；**之后无 CrossCore**
3. **sync_audit**：见下；无真死等红线。

## sync_audit 摘要

| 项 | 值 |
|----|----|
| 命令 | `python3 thirdparty/cannbot-skills/ops/ascendc-sync-audit/scripts/sync_audit.py mmad_custom.cpp --check all --format json` |
| 输出 | `/opt/cursor/artifacts/e20-sync-audit.json` |
| SYNC-03 | **假阳性候选**：静态分析把 `FsmWait`/`FsmSet` 包装函数内 Set/Wait 判为「同侧」；实际调用分落 AIC / 双 AIV，SIM 8 轮可证配对可活 |
| SYNC-09 | 性能提示：`PipeBarrier<PIPE_ALL>` 偏粗（stub 可接受） |
| 真死等红线 | **无** |

## 日志 / 产物路径

| 跑次 | 路径 |
|------|------|
| SIM 默认 8 轮 | `/opt/cursor/artifacts/e20-default-sim.log` |
| sync_audit JSON | `/opt/cursor/artifacts/e20-sync-audit.json` |
| 用例 tee | `graph_tests/toys/toy-e20-postmark-tail/output/host_trace.log` |
| 文档 | 同目录 `TRACE.md`、`STATUS.md` |

## 对图谱节点的 effect

| node | effect | note |
|------|--------|------|
| `J-hang-after-full-trace` | **support** | 「全 Mark 后 AIC 早退 + AIV-only 尾包」结构在 SIM×8 可活；非粘性复现 |
| `J-sim-not-sticky` | **cite / reaffirm** | 本 stub 亦不粘 |
| `D-exp-e17` | **reuse** | 壳与 CrossCore 短序来自 E17；本刀只加早退+尾包 |

## 范围合规

- 仅新建/改 `toy-e20-postmark-tail/` + FEEDBACK outbox + toys INDEX + API 查阅索引一行；未改知识库 yaml；未 commit/push；未上机；未并行 SIM；未抄 Encrypt/tail_pack/frozen。
- Kernel 自写 stub；工程壳从 E17 复制再改 `mmad_custom.cpp`。
- API：CrossCore/PipeBarrier/DataCopy 复用既有记录；同轮写回索引 E20 一行。
