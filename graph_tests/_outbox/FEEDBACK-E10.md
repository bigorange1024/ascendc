# FEEDBACK-E10

| 字段 | 值 |
|------|----|
| task_id | E10 |
| verdict | **PASS** |
| wall_clock_min | **~12**（CPU ~10s + SIM kernel ~105s；deadline 40） |
| directory | `graph_tests/toys/toy-e10-chain-plus-byteencode/` |
| hypothesis | `D-exp-e10` |

## 结果摘要

1. **默认 3 轮**：`SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` → **全绿**（不挂）。
2. Host TRACE：`100/101/105/110/111` 各 3。
3. L1 **真 SHAKE256** → **真 CBD(η=2)**：`200→210→211→212→220→221→203` ×3。
4. L2 **真 NTT** + **真 basemul** + **真 INTT** + **真 Compress_d(d=4)** + **真 ByteEncode_d(128B)** + SET(4)：AIV0 `500→…→552→560→562→502`；AIV1 对称至 Compress；AIC `401/402`。
5. **ByteEncode**：自包含拷贝 `vendor/byteencode_d/`（只读参考 `pass-f203-byteencode-d-vec-k4`）；Compress 后 AIV0 整 poly `poly_byte_encode_local`；非 TRACE stub。
6. **golden 对拍**：
   - SHAKE：`diffs=0/32`
   - CBD：`diffs=0/256`
   - `dst` vs `golden`（ByteEncode∘Compress∘INTT∘basemul∘NTT(CBD)）：`diffs=0/128B`
7. SoftSync：默认未加。SIM kernel wall ≈ **104.6s**（budget 900）；Total tick **748895**。
8. CPU 孪生亦全绿（`bash run.sh -r cpu`）。

## 日志路径

| 跑次 | 路径 |
|------|------|
| CPU | `/opt/cursor/artifacts/e10-cpu.log` |
| 默认 3 轮 SIM | `/opt/cursor/artifacts/e10-default-sim.log` |
| 用例 tee | `graph_tests/toys/toy-e10-chain-plus-byteencode/output/host_trace.log` |
| 文档 | 同目录 `TRACE.md`、`STATUS.md`、`ORIGIN-byteencode.md` |

## 对图谱节点的 effect

| node | effect | note |
|------|--------|------|
| `D-exp-e10` | **support** | E09 壳 + Compress 后 **真 ByteEncode_d(128B)** + SET(4) + ≥3 轮 SIM 不挂 + golden |
| `F-e09-compress-chain-sim-pass` | cite | 2-launch / Host μ / 真链壳复用 |
| `J-sim-rewrite-blocks-ready` | **support** | ByteEncode 积木自包含拷贝；短链可接 |
| `D-use-blocks` | **support** | Compress+ByteEncode 积木自包含；未改原探针 |
| `D-short-experiments` | **honored** | 墙钟 ~12min ≪ 40 |
| `D-no-repeat-retracted` | **honored** | 未测双 Cube / GATE alone / OMIT_SET4 / SoftSync 复测 |

## 范围合规

- 仅改白名单 `toy-e10-chain-plus-byteencode/` + 本 FEEDBACK outbox。
- 未改 E01–E09、Encrypt、原 ByteEncode/Compress 探针、图谱 yaml。
- 写 AscendC 前已读 ascendc-engineering-notes；API 均为已登记复用（ByteEncode mask/pack=`Muls`/`ShiftRight`/`Sub`；Compress 同 E09）。
