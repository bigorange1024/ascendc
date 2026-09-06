# FEEDBACK-E12

| 字段 | 值 |
|------|----|
| task_id | E12 |
| verdict | **PASS** |
| wall_clock_min | **~25**（CPU ~11s + SIM kernel ~206s；deadline 40） |
| directory | `graph_tests/toys/toy-e12-chain-k2-multipoly/` |
| hypothesis | `D-exp-e12` |

## 结果摘要

1. **默认 3 轮**：`SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` → **全绿**（不挂）。
2. Host TRACE：`100/101/105/110/111` 各 3。
3. L1 **真 SHAKE** + **真 CBD×2**：`200→…→222→203` ×3。
4. L2 **k=2 串行真链** ×2 + **真 Decompress_1(μ)** + Compress + ByteEncode + **一次 SET(4)**：
   - poly0 AIV0 `500→…→562→503`；poly1 `600→…→662→603`
   - AIC `400→401` / `410→411` / `402`
   - SET(4) `502/512` ×3
5. **k=2 真几何**：src 512 int32、prf 256B、g 512 int32、out **256B**（非 TRACE stub）。
6. **Decompress_1(μ)**：**保留**；μ 32B 两 poly **共享**（见 `ORIGIN-k2.md`）。
7. **golden 对拍**：
   - SHAKE：`diffs=0/32`
   - CBD：`diffs=0/512`
   - `dst` vs `golden`：**`diffs=0/256B`**
8. SIM kernel wall ≈ **206.1s**（budget 1200）；Total tick **1370677**；stray 已收拢至 `sim_log/`。
9. CPU 孪生亦全绿。

## 日志路径

| 跑次 | 路径 |
|------|------|
| CPU | `/opt/cursor/artifacts/e12-cpu.log` |
| 默认 3 轮 SIM | `/opt/cursor/artifacts/e12-default-sim.log` |
| 用例 tee | `graph_tests/toys/toy-e12-chain-k2-multipoly/output/host_trace.log` |
| 文档 | 同目录 `TRACE.md`、`STATUS.md`、`ORIGIN-k2.md` |

## 对图谱节点的 effect

| node | effect | note |
|------|--------|------|
| `D-exp-e12` | **support** | E11 壳扩 k=2 两路 poly 真链 + 2-launch + SET(4) + ≥3 轮 SIM 不挂 + golden 256B |
| `F-e11-decompress-mu-sim-pass` | cite | E11 壳与 Decompress/Compress/ByteEncode 积木复用 |
| `J-sim-rewrite-blocks-ready` | **support** | k=2 几何下积木仍自包含可接；L2 串行 poly 未挂 |
| `D-use-blocks` | **support** | 未改原探针；仅新 toy 目录 |
| `D-short-experiments` | **honored** | 墙钟 ~25min ≪ 40 |
| `D-no-repeat-retracted` | **honored** | 未测 retracted / Encrypt 整图 |

## 范围合规

- 仅改白名单 `toy-e12-chain-k2-multipoly/` + 本 FEEDBACK outbox。
- 未改 E01–E11、Encrypt、原探针、图谱 yaml。
- 未 commit/push。

## 实现要点（供主控）

- L2 poly **串行**（AIC/AIV 同迭代），共享 NTT/INTT CrossCore；末 poly 后单次 SET(4)。
- CBD k=2：`GlobalTensor` 全长 + `row` 索引（禁止裸指针偏移 + row=0，否则 CPU SIM 越界）。
- TRACE：poly 段尾用 503/603；502/512 仅 SET(4) 一次（避免 verify 双计数）。
