# FEEDBACK-E18

| 字段 | 值 |
|------|----|
| task_id | E18 |
| verdict | **PASS** |
| wall_clock_min | **~3**（SIM+审计；deadline 35） |
| directory | `graph_tests/toys/toy-e18-l18-fsm-sep56/` |
| hypothesis | `D-exp-e18` / H-sep56（相对 E17：INTT 独立 5/6） |
| rounds | **8**（`TOY_ROUNDS` 默认） |
| kernel_wall_sec | **100.764**（budget 600；rc=0 非 124） |
| total_wall_sec | **108.15**（含 cmake/build） |

## 结果摘要

1. **默认 8 轮**：`cd graph_tests/toys/toy-e18-l18-fsm-sep56 && SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` → **全绿**。
   - Host TRACE：`100`×8、`111`×8（末码 `111`）
   - 设备：每轮 AIC `400/401/403/410/411/418/420/421/423`；AIV0 `500/501/503/504/508/521/523`；AIV1 `510/511/513/514/518/531/533`
   - magic `E18TOY01` + `0xE8` OK；用例根无 stray dump
2. **sync_audit**：见下；无真死等红线。
3. **相对 E17 差分确认**：伪 NTT 仍 1/3、GATE 仍 4/8；伪 INTT 仅改为 AIV SET(5)↔AIC Wait(5)、AIC SET(6)↔AIV Wait(6)。

## sync_audit 摘要

| 项 | 值 |
|----|----|
| 命令 | `python3 thirdparty/cannbot-skills/ops/ascendc-sync-audit/scripts/sync_audit.py mmad_custom.cpp --check all --format json` |
| 输出 | `/opt/cursor/artifacts/e18-sync-audit.json` |
| SYNC-03 | **假阳性候选**：静态分析把 `FsmWait`/`FsmSet` 包装函数内 Set/Wait 判为「同侧」；实际调用分落 AIC / 双 AIV 块，SIM 8 轮可证配对可活 |
| SYNC-09 | 性能提示：`PipeBarrier<PIPE_ALL>` 偏粗（stub 可接受） |
| 真死等红线 | **无** |

## 日志 / 产物路径

| 跑次 | 路径 |
|------|------|
| SIM 默认 8 轮 | `/opt/cursor/artifacts/e18-default-sim.log` |
| sync_audit JSON | `/opt/cursor/artifacts/e18-sync-audit.json` |
| 用例 tee | `graph_tests/toys/toy-e18-l18-fsm-sep56/output/host_trace.log` |
| 文档 | 同目录 `TRACE.md`、`STATUS.md` |

## 对图谱节点的 effect

| node | effect | note |
|------|--------|------|
| `D-exp-e18` | **support** | stub 全序（NTT1/3→GATE4/8→INTT 独立5/6）单 launch×8 SIM 绿 + 审计无真死等 |
| `F-encrypt-gap-inventory` | **support** | G1+G2 离线可活（独立 INTT flag 亦可拼装）；非粘性复现（对齐 `J-sim-not-sticky`） |
| `J-sim-not-sticky` | **cite / reaffirm** | 本 stub 亦不粘 |
| `J-use-cannbot-on-gap` | **support** | 已跑 sync_audit；SYNC-03 记假阳性 |
| `D-exp-e17` | **cite** | 工程壳复制自 E17；未改 E17 树 |

## 范围合规

- 仅改白名单目录 + 本 FEEDBACK outbox；未改 E17 / ENCRYPT_GAP / 知识库 / 图谱 yaml；未 commit/push；未上机；未并行 SIM；未抄 Encrypt 业务。
- Kernel 自写 stub（自 E17 复制后只改 INTT 5/6 + magic/TRACE）；工程壳仿 E17。
- API 查阅索引：CrossCore/PipeBarrier/DataCopy 已有仓内记录；白名单禁止改索引文件，本刀未写回。
