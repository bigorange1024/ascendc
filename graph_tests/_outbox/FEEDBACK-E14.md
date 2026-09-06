# FEEDBACK-E14

| 字段 | 值 |
|------|----|
| task_id | E14 |
| verdict | **PASS** |
| wall_clock_min | **~35**（SIM 3 轮 kernel ~420s 含编译；deadline 40） |
| directory | `graph_tests/toys/toy-e14-glue-plus-samplentt/` |
| hypothesis | `D-exp-e14` |

## 结果摘要

1. **3 轮 SIM**：`SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` → **全绿**（rc=0，不挂）。
2. **真 SampleNTT**：独立 launch（phase=1）；设备 `BuildAlg7SampleNttFromSeedD(SEED_D,(0,0)/(0,1))` → ws[G0/G1]；TRACE **300/302/303 ×3**。
3. **Encrypt 粘合保留**：L1 SHAKE+CBD(u×2+v) → SampleNTT → L2 u/v 真链 + **c1∥c2(384B)** + SET(4)；Host **100/101/102/104/106/105/110/111 ×3**。
4. **golden**：SHAKE 0/32；CBD(u) 0/512；**c 0/384B**（SampleNTT oracle + v 路 G2 stub）。
5. SIM kernel wall ≈ **420s**（budget 2000）；402×3、744/745×3 均齐。

## 矩阵覆盖（诚实）

| 元 | 设备 SampleNTT | 用途 |
|----|----------------|------|
| (0,0) | ✅ | u0 basemul |
| (0,1) | ✅ | u1 basemul |
| (1,0)/(1,1) | ❌ | 未做（墙钟优先 u 路 2 poly） |
| v G2 | Host stub | 同 E13 |

## 日志路径

| 跑次 | 路径 |
|------|------|
| SIM 3 轮（验收） | `/opt/cursor/artifacts/e14-default-sim.log` |
| SIM 1 轮调试 | `/opt/cursor/artifacts/e14-sim-r1.log` |
| 用例 tee | `graph_tests/toys/toy-e14-glue-plus-samplentt/output/host_trace.log` |
| 文档 | `TRACE.md` / `STATUS.md` / `ORIGIN-samplentt.md` |

## 对图谱节点的 effect

| node | effect | note |
|------|--------|------|
| `D-exp-e14` | **support** | E13 壳 + 真 SampleNTT(u×2) + ≥3 轮 SIM 不挂 + golden |
| `F-e13-encrypt-shaped-glue-sim-pass` | cite | 粘合壳来源 |
| `J-sim-rewrite-blocks-ready` | **support** | 独立 SampleNTT launch + L2 积木仍绿 |
| `D-use-blocks` | **support** | 仅新 toy + vendor/sample_ntt |
| `D-short-experiments` | **honored** | 墙钟 ~35min ≤40 |
| `D-no-copy-encrypt` | **honored** | 未抄 Encrypt |
| `D-no-repeat-retracted` | **honored** | 未测 retracted |

## 实现要点

- **3-launch Host**：L1 → **SampleNTT phase**（AIV subBlock0 only）→ μ → L2；避免 MIX 内嵌 SampleNTT TPipe 与真链互抢导致 SIM 挂死。
- `vendor/sample_ntt/f203_alg7_d12_vec.hpp`：`d1/d2 GM nullptr` 时跳过写出（粘合仅需 â）。
- CPU 孪生：SampleNTT×2/轮极慢 + MIX 顺序模型易堵；**本 TASK 以 SIM 为准**（SUBAGENT_RULES SIM only）。

## 范围合规

- 仅改白名单 `toy-e14-glue-plus-samplentt/` + 本 FEEDBACK。
- 未改 E01–E13、Encrypt、原探针、图谱 yaml。
- 未 commit/push。
