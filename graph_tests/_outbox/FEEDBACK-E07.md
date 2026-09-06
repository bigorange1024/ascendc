# FEEDBACK-E07

| 字段 | 值 |
|------|----|
| task_id | E07 |
| verdict | **PASS** |
| wall_clock_min | **~11.6**（issued 08:00Z → SIM 完成 ~08:11Z；deadline 40） |
| directory | `graph_tests/toys/toy-e07-shake-ntt-basemul-intt/` |
| hypothesis | `D-exp-e07` |

## 结果摘要

1. **默认 3 轮**：`SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` → **全绿**（不挂）。
2. Host TRACE：`100/101/105/110/111` 各 3。
3. L1 **真 SHAKE256**：`200→210→211→212→203` ×3。
4. L2 **真 NTT** + **真 basemul** + **真 INTT** + SET(4)：
   - NTT：`520/521`；basemul：`530→532` / `531→533`；INTT：`540→542` / `541→543`；壳层 `401/402` + `502/512`。
5. **INTT 语义**：**E04 同系**（ntt256 矩阵逆 Minv）；**≠ Tag5T**（未整图拷贝 polyvec8；STATUS/ORIGIN-intt 已写明）。
6. **golden 对拍**：
   - SHAKE `shake_y.bin` vs `shake_golden.bin`：**diffs=0/32**
   - `dst.bin` vs `golden.bin`：**diffs=0/256**（`INTT(Alg.11(NTT(src),ĝ))`）
7. SoftSync：默认未加。kernel wall ≈ **96.4s**（budget 900）；Total tick **584061**。
8. 工程点：INTT 与 NTT 分作用域 `AivSplit`/`AivMerge`（避免叠 TQue）；Merge 后半区标量 Barrett canonical（修二次 Merge 偶发未约化 lane）。

## 日志路径

| 跑次 | 路径 |
|------|------|
| CPU | `/opt/cursor/artifacts/e07-cpu2.log` |
| 默认 3 轮 SIM | `/opt/cursor/artifacts/e07-default-sim.log` |
| 用例 tee | `graph_tests/toys/toy-e07-shake-ntt-basemul-intt/output/host_trace.log` |
| 文档 | 同目录 `TRACE.md`、`STATUS.md`、`ORIGIN-intt.md` |

## 对图谱节点的 effect

| node | effect | note |
|------|--------|------|
| `D-exp-e07` | **support** | E06 壳 + 真 SHAKE+NTT+basemul+**真 INTT**+SET(4) + ≥3 轮 SIM 不挂 + golden |
| `F-e06-shake-ntt-basemul-sim-pass` | cite | 2-launch / Host μ / 真 SHAKE+NTT+basemul+SET4 壳复用 |
| `D-use-blocks` | **support** | ntt256 同系 Minv INTT + basemul 积木自包含；未改原探针 |
| `D-short-experiments` | **honored** | 墙钟 ~11.6min ≪ 40 |
| `D-no-repeat-retracted` | **honored** | 未测双 Cube / GATE alone / OMIT_SET4 / SoftSync 复测 |

## 范围合规

- 仅改白名单 `toy-e07-shake-ntt-basemul-intt/` + 本 FEEDBACK outbox（及 toys INDEX / 当日 qa / HANDOFF）。
- 未改 E01–E06、Encrypt、原 Tag5T/ntt256 探针、图谱 yaml。
- 写 AscendC 前已读 ascendc-engineering-notes；API 均为已登记复用。
