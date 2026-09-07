# FEEDBACK-E05

| 字段 | 值 |
|------|----|
| task_id | E05 |
| verdict | **PASS** |
| wall_clock_min | **~5.2**（issued 07:46Z → SIM 完成 ~07:51Z；deadline 40） |
| directory | `graph_tests/toys/toy-e05-skel-shake-plus-ntt/` |
| hypothesis | `D-exp-e05` |

## 结果摘要

1. **默认 3 轮**：`SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` → **全绿**（不挂）。
2. Host TRACE：`100/101/105/110/111` 各 3。
3. L1 **真 SHAKE256**：`200→210→211→212→203` ×3（非空 TRACE stub；212=UB golden PASS）。
4. L2 **真 NTT**（自包含 ntt256）：AIV Split → AIC Mmad×2 → AIV Merge+Barrett；TRACE `520/521`；壳层 SET(4) `401/402` + `502/512`。
5. **golden 对拍**：
   - SHAKE 短向量 `shake_y.bin` vs `shake_golden.bin`：**diffs=0/32**（`hashlib.shake_256(b"abc").digest(32)`）
   - NTT `dst.bin` vs `golden.bin`：**diffs=0/256**（`ntt_sim_kyber`；**≠ Tag5T**）
6. SoftSync：默认未加。kernel wall ≈ **59.0s**（budget 900）；Total tick **459141**。
7. SHAKE 积木：本目录 `vendor/shake_xof_kernel/` + `vendor/keccak_f1600_kernel/` 自包含拷贝；未改原 shared / `pass-shake256-ascendc-toy`。

## 日志路径

| 跑次 | 路径 |
|------|------|
| 默认 3 轮 SIM | `/opt/cursor/artifacts/e05-default-sim.log` |
| 用例 tee | `graph_tests/toys/toy-e05-skel-shake-plus-ntt/output/host_trace.log` |
| 文档 | 同目录 `TRACE.md`、`STATUS.md` |

## 对图谱节点的 effect

| node | effect | note |
|------|--------|------|
| `D-exp-e05` | **support** | E04 壳 + L1 真 SHAKE256 + L2 真 NTT + SET(4) + ≥3 轮 SIM 不挂 + shake/ntt golden |
| `F-e04-skel-real-ntt-sim-pass` | cite | 2-launch / Host μ / 真 NTT+SET4 壳复用 |
| `D-layer-real-compute` | **support** | L1+L2 均为真计算路径（非 TRACE-only stub） |
| `D-use-blocks` | **support** | SHAKE/Keccak 与 ntt256 积木自包含拷贝进本目录，未改原目录 |
| `D-short-experiments` | **honored** | 墙钟 ~5min ≪ 40 |
| `D-no-repeat-retracted` | **honored** | 未测双 Cube / GATE alone / OMIT_SET4 / SoftSync 复测 |

## 范围合规

- 仅改白名单 `toy-e05-skel-shake-plus-ntt/` + 本 FEEDBACK outbox。
- 未改 E01–E04、Encrypt、shared 原文件、图谱 yaml、知识库查阅索引（白名单外）。
- 未抄 Encrypt；未复测 retracted。
- 写 AscendC 前已读 ascendc-engineering-notes；API 均为已登记复用（`ProcessInline` + TPipe/DataCopy 等）。
