# FEEDBACK-E04

| 字段 | 值 |
|------|----|
| task_id | E04 |
| verdict | **PASS** |
| wall_clock_min | **~5**（issued 07:40Z → SIM 完成 ~07:45Z；deadline 40） |
| directory | `graph_tests/toys/toy-e04-skel-plus-real-ntt/` |
| hypothesis | `D-exp-e04` |

## 结果摘要

1. **默认 3 轮**：`SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` → **全绿**（不挂）。
2. Host TRACE：`100/101/105/110/111` 各 3。
3. L1 采样 stub：`200→201→202→203` ×3。
4. L2 **真 NTT**（自包含 ntt256）：AIV Split → AIC Mmad×2 → AIV Merge+Barrett；TRACE `520/521` 非空 stub；壳层 SET(4) `401/402` + `502/512`。
5. **golden 对拍**：`dst.bin` vs `golden.bin` **diffs=0/256**（`ntt_sim_kyber` / merged_kyber 语义）。
6. **本 NTT golden ≠ F203 Tag5T**（STATUS 已写明）。
7. SoftSync：默认未加。kernel wall ≈ **54.6s**（budget 900）。

## 日志路径

| 跑次 | 路径 |
|------|------|
| 默认 3 轮 SIM | `/opt/cursor/artifacts/e04-default-sim.log` |
| 用例 tee | `graph_tests/toys/toy-e04-skel-plus-real-ntt/output/host_trace.log` |
| 文档 | 同目录 `TRACE.md`、`STATUS.md` |

## 对图谱节点的 effect

| node | effect | note |
|------|--------|------|
| `D-exp-e04` | **support** | E03 壳 + L2 真单 poly NTT + SET(4) + ≥3 轮 SIM 不挂 + ntt256 golden 对拍 |
| `F-e03-stage-skel-sim-pass` | cite | 2-launch / L1 stub / Host μ / SET4 壳复用 |
| `D-layer-real-compute` | **support** | L2 已换真 NTT 计算路径（非 TRACE-only stub） |
| `D-use-blocks` | **support** | 积木自包含拷贝自 `pass-merged-kyber-mix-ntt256/`，未改原目录 |
| `D-short-experiments` | **honored** | 墙钟 ~5min ≪ 40 |
| `D-no-repeat-retracted` | **honored** | 未测双 Cube / GATE alone / OMIT_SET4 / SoftSync 复测 |

## 范围合规

- 仅改白名单 `toy-e04-skel-plus-real-ntt/` + 本 FEEDBACK outbox。
- 未改 E01–E03、Encrypt、ntt256 原目录、图谱 yaml、知识库。
- 未抄 Encrypt；未复测 retracted。
