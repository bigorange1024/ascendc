# FEEDBACK-E01

| 字段 | 值 |
|------|----|
| task_id | E01 |
| verdict | **PASS** |
| wall_clock_min | **~6**（issued 07:20Z → 完成 ~07:25Z；deadline 35） |
| directory | `graph_tests/toys/toy-e01-2launch-set4-trace-repeat/` |
| hypothesis | `D-exp-e01` |

## 结果摘要

1. **默认 8 轮**：`SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` → **全绿**；Host TRACE `100/101/110/111` 各 8；L2 设备 `400/401/402`、`500/502`、`510/512`；magic OK；kernel wall ≈ **75s**（budget 600）。
2. **OMIT_SET4**：`KERNEL_COMPUTE_BUDGET_SEC=60 TOY_ROUNDS=1 OMIT_SET4=1 SIM_DIRECT=1 bash run.sh -r sim` → **rc=124**（预期挂）；Host 停在 `110` 后无 `111`。

## 日志路径

| 跑次 | 路径 |
|------|------|
| 默认 8 轮 | `/opt/cursor/artifacts/e01-default-sim.log` |
| OMIT_SET4 | `/opt/cursor/artifacts/e01-omit-set4-sim.log` |
| 用例 tee（末次=OMIT） | `graph_tests/toys/toy-e01-2launch-set4-trace-repeat/output/host_trace.log` |
| 文档 | 同目录 `TRACE.md`、`STATUS.md` |

## 对图谱节点的 effect

| node | effect | note |
|------|--------|------|
| `D-exp-e01` | **support** | 新目录 2-launch+SET4+数字 TRACE×8 轮 SIM 全完成；交付可跑壳 |
| `F-omit-set4-sim124` | **cite / reaffirm** | 本 toy 上 OMIT_SET4⇒124，与冻结证据同构（非发现型复踩） |
| `F-set4-ok-sim` | **cite / reaffirm** | 默认 SET(4) 可达则绿 |
| `D-layer-handshake` | support（间接） | 握手不变量在新目录可拼装复用 |
| `D-no-repeat-retracted` | **honored** | 未测双 Cube / GATE alone / launch 次数假说 |
| `D-short-experiments` | **honored** | 墙钟约 6min ≪ 35 |

## 范围合规

- 仅改白名单目录 + 本 FEEDBACK outbox；未改 stable/冻结用例/图谱 yaml/知识库。
- Kernel 自写极简（无 Encrypt/NTT/μ）；工程壳仿 skel/clean 的 CMake/`run.sh`。
