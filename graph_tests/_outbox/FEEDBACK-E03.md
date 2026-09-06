# FEEDBACK-E03

| 字段 | 值 |
|------|----|
| task_id | E03 |
| verdict | **PASS** |
| wall_clock_min | **~4.5**（issued 07:33Z → 完成 ~07:37Z；deadline 35） |
| directory | `graph_tests/toys/toy-e03-stage-skel-2launch/` |
| hypothesis | `D-exp-e03` |

## 结果摘要

1. **默认 3 轮**：`SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` → **全绿**。
2. Host TRACE：`100/101/105/110/111` 各 3（105 = Host μ 空操作）。
3. L1 采样 stub：`200→201→202→203` ×3（无真 SHAKE）。
4. L2 代数 stub + SET(4)：AIC `400/401/402`；AIV0 `500→520→530→540→502`；AIV1 `510→521→531→541→512`；magic OK。
5. SoftSync：**默认未加**（跟 E02 weaken：极简 SET4 骨架非必要）。
6. kernel wall ≈ **44s**（budget 600）。

## 日志路径

| 跑次 | 路径 |
|------|------|
| 默认 3 轮 SIM | `/opt/cursor/artifacts/e03-default-sim.log` |
| 用例 tee | `graph_tests/toys/toy-e03-stage-skel-2launch/output/host_trace.log` |
| 文档 | 同目录 `TRACE.md`、`STATUS.md` |

## 对图谱节点的 effect

| node | effect | note |
|------|--------|------|
| `D-exp-e03` | **support** | 新目录 Encrypt 形态骨架：L1 采样 stub + L2 代数 stub + SET(4) + 数字 TRACE×3 轮 SIM 全完成；阶段顺序可读 |
| `F-e02-softsync-set4-sim-pass` | cite | 工程壳/SET4 思路复用；本刀默认不加 SoftSync |
| `D-host-mu-default` | **support** | Host 105 μ 空操作可插入于 L1 与 L2 之间，不影响 SIM 绿 |
| `D-use-blocks` | cite | 骨架按采样/代数阶段块拼装，非一次性复刻 Encrypt |
| `D-short-experiments` | **honored** | 墙钟约 4.5min ≪ 35 |
| `D-no-repeat-retracted` | **honored** | 未测双 Cube / GATE alone / OMIT_SET4 |

## 范围合规

- 仅改白名单目录 + 本 FEEDBACK outbox；未改 E01/E02/stable/冻结用例/图谱 yaml/知识库。
- Kernel 自写 stub（无 Encrypt 业务抄码、无真算）；工程壳只读仿 E01。
- 默认无 SoftSync；未复测 retracted 假说。
