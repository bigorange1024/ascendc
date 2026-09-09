# FEEDBACK-E17

| 字段 | 值 |
|------|----|
| task_id | E17 |
| verdict | **PASS** |
| wall_clock_min | **~5**（SIM+审计；deadline 40） |
| directory | `graph_tests/toys/toy-e17-l18-fsm-reuse13/` |
| hypothesis | `D-exp-e17` / H-reuse（G1+G2 stub 序） |
| rounds | **8**（`TOY_ROUNDS` 默认） |
| kernel_wall_sec | **80.681**（budget 600；rc=0 非 124） |

## 结果摘要

1. **默认 8 轮**：`cd graph_tests/toys/toy-e17-l18-fsm-reuse13 && SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` → **全绿**。
   - Host TRACE：`100`×8、`111`×8（末码 `111`）
   - 设备：每轮 AIC `400/401/403/410/411/418/420/421/423`；AIV0 `500/501/503/504/508/521/523`；AIV1 `510/511/513/514/518/531/533`
   - magic `E17TOY01` + `0xE7` OK；用例根无 stray dump
2. **sync_audit**：见下；无真死等红线。
3. **未做 E18**（下一单：INTT 改 5/6）。

## sync_audit 摘要

| 项 | 值 |
|----|----|
| 命令 | `python3 thirdparty/cannbot-skills/ops/ascendc-sync-audit/scripts/sync_audit.py mmad_custom.cpp --check all --format json` |
| 输出 | `/opt/cursor/artifacts/e17-sync-audit.json` |
| SYNC-03 | **假阳性候选**：静态分析把 `FsmWait`/`FsmSet` 包装函数内 Set/Wait 判为「同侧」；实际调用分落 AIC / 双 AIV 块，SIM 8 轮可证配对可活 |
| SYNC-09 | 性能提示：`PipeBarrier<PIPE_ALL>` 偏粗（stub 可接受） |
| 真死等红线 | **无** |

## 日志 / 产物路径

| 跑次 | 路径 |
|------|------|
| SIM 默认 8 轮 | `/opt/cursor/artifacts/e17-default-sim.log` |
| sync_audit JSON | `/opt/cursor/artifacts/e17-sync-audit.json` |
| 用例 tee | `graph_tests/toys/toy-e17-l18-fsm-reuse13/output/host_trace.log` |
| 文档 | 同目录 `TRACE.md`、`STATUS.md` |

## 对图谱节点的 effect

| node | effect | note |
|------|--------|------|
| `D-exp-e17` | **support** | stub 全序（NTT1/3→GATE4/8→INTT 复用1/3）单 launch×8 SIM 绿 + 审计无真死等 |
| `F-encrypt-gap-inventory` | **support** | G1+G2 离线可活（结构可拼装）；非粘性复现（对齐 `J-sim-not-sticky`） |
| `J-sim-not-sticky` | **cite / reaffirm** | 本 stub 亦不粘 |
| `J-use-cannbot-on-gap` | **support** | 已跑 sync_audit；SYNC-03 记假阳性 |
| `D-exp-e18` | — | **未做**（下一单） |

## 范围合规

- 仅改白名单目录 + 本 FEEDBACK outbox；未改 ENCRYPT_GAP / 知识库 / 图谱 yaml；未 commit/push；未上机；未并行 SIM；未抄 Encrypt 业务。
- Kernel 自写 stub；工程壳仿 E01（单 launch 改编）。
- API 查阅索引：CrossCore/PipeBarrier/DataCopy 已有仓内记录；白名单禁止改索引文件，本刀未写回。
