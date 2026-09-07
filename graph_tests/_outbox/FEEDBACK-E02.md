# FEEDBACK-E02

| 字段 | 值 |
|------|----|
| task_id | E02 |
| verdict | **PASS** |
| wall_clock_min | **~5**（issued 07:27Z → 完成 ~07:32Z；deadline 30） |
| directory | `graph_tests/toys/toy-e02-softsync-then-set4/` |
| hypothesis | `D-exp-e02` |

## 结果摘要

1. **默认 3 轮**：`SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` → **全绿**；Host TRACE `100/101/110/111` 各 3；L2 设备 `400/401/402`、`500/503/502`、`510/513/512`（SoftSync→SET 顺序）；magic OK；kernel wall ≈ **37s**（budget 600）。
2. **可选 OMIT_SOFTSYNC**：`TOY_ROUNDS=3 OMIT_SOFTSYNC=1 SIM_DIRECT=1 bash run.sh -r sim` → **仍绿** → 本极简骨架上 SoftSync **非必要**（**weaken**，非 FAIL）；SET(4) 仍足以放行 AIC。

## 日志路径

| 跑次 | 路径 |
|------|------|
| 默认 SoftSync→SET4 ×3 | `/opt/cursor/artifacts/e02-default-sim.log` |
| OMIT_SOFTSYNC | `/opt/cursor/artifacts/e02-omit-softsync-sim.log` |
| 用例 tee（末次=OMIT） | `graph_tests/toys/toy-e02-softsync-then-set4/output/host_trace.log` |
| 文档 | 同目录 `TRACE.md`、`STATUS.md` |

## 对图谱节点的 effect

| node | effect | note |
|------|--------|------|
| `D-exp-e02` | **support** | 新目录双 AIV SoftSyncArrive→SET(4)+AIC Wait(4)+数字 TRACE×3 轮 SIM 全完成 |
| `F-e01-8round-sim-pass` | cite | 复用 E01 工程壳思路（未改 E01） |
| `D-softsync-follow` | **support** | SoftSync 跟 decrypt skel 单向定式（AIV0 写哨兵 / AIV1 自旋），未自造双向 |
| `D-set4-invariant` | cite / reaffirm | SET(4) 仍为 AIC 放行关键；OMIT SoftSync 仍靠 SET(4) 绿 |
| `D-short-experiments` | **honored** | 墙钟约 5min ≪ 30；未做双 Cube/GATE alone/OMIT_SET4 |

### weaken（可选对照，非 FAIL）

| node / 说法 | effect | note |
|-------------|--------|------|
| 「本骨架 SoftSync 为必要前置」 | **weaken** | `OMIT_SOFTSYNC=1` 仍绿 → SoftSync 对本极简 SET4 toy **非必要**；生产 fused 仍可保留 SoftSync 作业务前置 |

## 范围合规

- 仅改白名单目录 + 本 FEEDBACK outbox；未改 E01/stable/冻结用例/图谱 yaml/知识库。
- SoftSync 单向定式；禁止双向 SoftSync；未复测 OMIT_SET4 / 双 Cube / GATE alone。
- Kernel 自写极简（无 Encrypt/NTT/μ）；工程壳仿 E01。
