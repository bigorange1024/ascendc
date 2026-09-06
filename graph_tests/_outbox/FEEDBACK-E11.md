# FEEDBACK-E11

| 字段 | 值 |
|------|----|
| task_id | E11 |
| verdict | **PASS** |
| wall_clock_min | **~15**（CPU ~3s + SIM kernel ~122s；deadline 40） |
| directory | `graph_tests/toys/toy-e11-chain-plus-decompress-mu/` |
| hypothesis | `D-exp-e11` |

## 结果摘要

1. **默认 3 轮**：`SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` → **全绿**（不挂）。
2. Host TRACE：`100/101/105/110/111` 各 3。
3. L1 **真 SHAKE256** → **真 CBD(η=2)**：`200→210→211→212→220→221→203` ×3。
4. L2 **真 NTT** + **真 basemul** + **真 INTT** + **真 Decompress_1(μ)** + **真 Compress_d(d=4)** + **真 ByteEncode_d(128B)** + SET(4)：
   - AIV0 `500→…→542→544→546→550→552→560→562→502`
   - AIV1 对称至 Compress（545/547/551/553）
   - AIC `401/402`
5. **Decompress_1(μ)**：自包含 `vendor/decompress_d/` + d=1 扩展（`decompress_d1_mu_embed.hpp` / `decompress_d1_ref.c`）；Host `mu.bin` 32B（SEED_D+1）；非 TRACE stub。
6. **golden 对拍**：
   - SHAKE：`diffs=0/32`
   - CBD：`diffs=0/256`
   - `dst` vs `golden`（ByteEncode∘Compress∘Decompress_1(μ)∘INTT∘basemul∘NTT(CBD)）：`diffs=0/128B`
7. SIM kernel wall ≈ **121.8s**（budget 900）；Total tick **828718**；stray dump 已收拢至 `sim_log/`。
8. CPU 孪生亦全绿（`bash run.sh -r cpu`）。

## 日志路径

| 跑次 | 路径 |
|------|------|
| CPU | `/opt/cursor/artifacts/e11-cpu.log` |
| 默认 3 轮 SIM | `/opt/cursor/artifacts/e11-default-sim.log` |
| 用例 tee | `graph_tests/toys/toy-e11-chain-plus-decompress-mu/output/host_trace.log` |
| 文档 | 同目录 `TRACE.md`、`STATUS.md`、`ORIGIN-decompress.md` |

## 对图谱节点的 effect

| node | effect | note |
|------|--------|------|
| `D-exp-e11` | **support** | E10 壳 + INTT 后 **真 Decompress_1(μ)** + Compress/ByteEncode + ≥3 轮 SIM 不挂 + golden |
| `F-e10-byteencode-chain-sim-pass` | cite | E10 真链壳复用 |
| `J-sim-rewrite-blocks-ready` | **support** | Decompress_1 积木自包含拷贝 + d=1 扩展；短链可接 |
| `D-use-blocks` | **support** | Decompress+Compress+ByteEncode 积木；未改原探针 |
| `D-short-experiments` | **honored** | 墙钟 ~15min ≪ 40 |
| `D-no-repeat-retracted` | **honored** | 未测 retracted / Encrypt 整图 |

## 范围合规

- 仅改白名单 `toy-e11-chain-plus-decompress-mu/` + 本 FEEDBACK outbox。
- 未改 E01–E10、Encrypt、原 Decompress/Compress/ByteEncode 探针、图谱 yaml。
- 写 AscendC 前已读 ascendc-engineering-notes；API 均为已登记复用（DataCopy/Muls/Adds/ShiftRight/BarrettRed 等同 E09/E10）。
