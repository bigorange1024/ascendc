# FEEDBACK-E06

| 字段 | 值 |
|------|----|
| task_id | E06 |
| verdict | **PASS** |
| wall_clock_min | **~5.6**（issued 07:53Z → SIM 完成 ~07:58Z；deadline 40） |
| directory | `graph_tests/toys/toy-e06-shake-ntt-basemul/` |
| hypothesis | `D-exp-e06` |

## 结果摘要

1. **默认 3 轮**：`SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` → **全绿**（不挂）。
2. Host TRACE：`100/101/105/110/111` 各 3。
3. L1 **真 SHAKE256**：`200→210→211→212→203` ×3。
4. L2 **真 NTT** + **真 basemul/MultiplyNTTs**（标量 Alg.11/12，非 TRACE stub）+ SET(4)：
   - NTT：`520/521`；basemul：`530→532` / `531→533`；壳层 `401/402` + `502/512`。
5. **golden 对拍**：
   - SHAKE `shake_y.bin` vs `shake_golden.bin`：**diffs=0/32**
   - MultiplyNTTs `dst.bin` vs `golden.bin`：**diffs=0/256**（`ĥ=Alg.11(NTT(src),ĝ)`；ĝ=`(13i+7)%q`）
6. SoftSync：默认未加。kernel wall ≈ **66.5s**（budget 900）；Total tick **508254**。
7. 积木：`vendor/basemul_scalar/` 自包含拷贝 γ 表 + 标量半区路径；参考 `pass-fix-f203-alg11-12-multiplyntts-k4`（未改原目录）；未抄 Encrypt。

## 日志路径

| 跑次 | 路径 |
|------|------|
| 默认 3 轮 SIM | `/opt/cursor/artifacts/e06-default-sim.log` |
| 用例 tee | `graph_tests/toys/toy-e06-shake-ntt-basemul/output/host_trace.log` |
| 文档 | 同目录 `TRACE.md`、`STATUS.md` |

## 对图谱节点的 effect

| node | effect | note |
|------|--------|------|
| `D-exp-e06` | **support** | E05 壳 + L1 真 SHAKE + L2 真 NTT + **真 basemul** + SET(4) + ≥3 轮 SIM 不挂 + shake/basemul golden |
| `F-e05-shake-ntt-sim-pass` | cite | 2-launch / Host μ / 真 SHAKE+NTT+SET4 壳复用 |
| `D-layer-real-compute` | **support** | L1+L2 NTT+basemul 均为真计算路径 |
| `D-use-blocks` | **support** | multiplyntts 标量积木自包含拷贝进本目录，未改原探针 |
| `D-short-experiments` | **honored** | 墙钟 ~5.6min ≪ 40 |
| `D-no-repeat-retracted` | **honored** | 未测双 Cube / GATE alone / OMIT_SET4 / SoftSync 复测 |

## 范围合规

- 仅改白名单 `toy-e06-shake-ntt-basemul/` + 本 FEEDBACK outbox。
- 未改 E01–E05、Encrypt、原 multiplyntts/innerproduct、图谱 yaml。
- 写 AscendC 前已读 ascendc-engineering-notes；API 均为已登记复用。
