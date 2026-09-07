# FEEDBACK-E09

| 字段 | 值 |
|------|----|
| task_id | E09 |
| verdict | **PASS** |
| wall_clock_min | **~7**（issued 08:24Z → SIM 完成 ~08:31Z；deadline 40） |
| directory | `graph_tests/toys/toy-e09-chain-plus-compress/` |
| hypothesis | `D-exp-e09` |

## 结果摘要

1. **默认 3 轮**：`SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` → **全绿**（不挂）。
2. Host TRACE：`100/101/105/110/111` 各 3。
3. L1 **真 SHAKE256** → **真 CBD(η=2)**：`200→210→211→212→220→221→203` ×3。
4. L2 **真 NTT** + **真 basemul** + **真 INTT** + **真 Compress_d(d=4)** + SET(4)：AIV0 `500→…→542→550→552→502`；AIV1 对称；AIC `401/402`。
5. **Compress**：自包含拷贝 `vendor/compress_d/`（只读参考 `pass-f203-compress-d-vec-k4`）；INTT 后双 AIV 各压 half（向量 Barrett d=4）；非 TRACE stub。
6. **golden 对拍**：
   - SHAKE：`diffs=0/32`
   - CBD：`diffs=0/256`
   - `dst` vs `golden`（Compress∘INTT∘basemul∘NTT(CBD)）：`diffs=0/256`
7. SoftSync：默认未加。kernel wall ≈ **97.0s**（budget 900）；Total tick **695150**。
8. CPU 孪生亦全绿（`bash run.sh -r cpu`）。

## 日志路径

| 跑次 | 路径 |
|------|------|
| CPU | `/opt/cursor/artifacts/e09-cpu.log` |
| 默认 3 轮 SIM | `/opt/cursor/artifacts/e09-default-sim.log` |
| 用例 tee | `graph_tests/toys/toy-e09-chain-plus-compress/output/host_trace.log` |
| 文档 | 同目录 `TRACE.md`、`STATUS.md`、`ORIGIN-compress.md` |

## 对图谱节点的 effect

| node | effect | note |
|------|--------|------|
| `D-exp-e09` | **support** | E08 壳 + 真 SHAKE+CBD+NTT+basemul+INTT+**真 Compress**+SET(4) + ≥3 轮 SIM 不挂 + golden |
| `F-e08-cbd-chain-sim-pass` | cite | 2-launch / Host μ / 真链壳复用 |
| `J-sim-rewrite-blocks-ready` | **support** | Compress_d 积木自包含拷贝；短链可接 |
| `D-use-blocks` | **support** | Compress 积木自包含；未改原探针 |
| `D-short-experiments` | **honored** | 墙钟 ~7min ≪ 40 |
| `D-no-repeat-retracted` | **honored** | 未测双 Cube / GATE alone / OMIT_SET4 / SoftSync 复测 |

## 范围合规

- 仅改白名单 `toy-e09-chain-plus-compress/` + 本 FEEDBACK outbox。
- 未改 E01–E08、Encrypt、原 Compress/Tag5T 探针、图谱 yaml。
- 写 AscendC 前已读 ascendc-engineering-notes；API 均为已登记复用（Compress Barrett=`Muls`/`Adds`/`ShiftRight`）。
