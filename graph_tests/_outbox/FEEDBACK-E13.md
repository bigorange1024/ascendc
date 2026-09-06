# FEEDBACK-E13

| 字段 | 值 |
|------|----|
| task_id | E13 |
| verdict | **PASS** |
| wall_clock_min | **~30**（CPU ~12s + SIM kernel ~253s；deadline 40） |
| directory | `graph_tests/toys/toy-e13-encrypt-shaped-glue/` |
| hypothesis | `D-exp-e13` |

## 结果摘要

1. **默认 3 轮**：`SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` → **全绿**（不挂，rc=0）。
2. Host TRACE：`100/101/102/105/110/111` 各 3（L1 采样 / L2 代数+压码 **真两段**）。
3. L1：**SHAKE** + **CBD u×2**（222）+ **CBD v**（223）×3。
4. L2 **Encrypt 粘合**：
   - u 路 c1：poly0/1 真链 **无 μ** → `out[0:256]`
   - v 路 c2：e2 真链 **含 Decompress_1(μ)** → `out[256:384]`
   - AIC `400/410/420` + **402×3**（Wait(4) 前；502/512 SIM 偶发丢失，见 TRACE.md）
5. **c 形输出**：384B = c1(256B)∥c2(128B)；布局见 `ORIGIN-glue.md` / `STATUS.md`。
6. **golden 对拍**：
   - SHAKE：`diffs=0/32`
   - CBD(u)：`diffs=0/512`
   - `dst` vs `golden`：**`diffs=0/384B`**
7. SIM kernel wall ≈ **291s**（budget 1500）；stray 已收拢至 `sim_log/`。
8. CPU 孪生亦全绿。

## 日志路径

| 跑次 | 路径 |
|------|------|
| CPU | `/opt/cursor/artifacts/e13-cpu.log` |
| 默认 3 轮 SIM | `/opt/cursor/artifacts/e13-default-sim.log` |
| 用例 tee | `graph_tests/toys/toy-e13-encrypt-shaped-glue/output/host_trace.log` |
| 文档 | 同目录 `TRACE.md`、`STATUS.md`、`ORIGIN-glue.md` |

## 对图谱节点的 effect

| node | effect | note |
|------|--------|------|
| `D-exp-e13` | **support** | E12 积木 + Encrypt 两段角色 + c1∥c2(384B) + ≥3 轮 SIM 不挂 + golden |
| `F-e12-k2-multipoly-sim-pass` | cite | E12 壳与真积木复用 |
| `J-sim-rewrite-blocks-ready` | **support** | u/v 分路 + μ 仅 v 路；积木仍自包含 |
| `D-use-blocks` | **support** | 仅新 toy 目录 |
| `D-short-experiments` | **honored** | 墙钟 ~30min ≪ 40 |
| `D-no-copy-encrypt` | **honored** | 未抄 Encrypt/Encaps 实现 |
| `D-no-repeat-retracted` | **honored** | 未测 retracted 路线 |

## 范围合规

- 仅改白名单 `toy-e13-encrypt-shaped-glue/` + 本 FEEDBACK outbox。
- 未改 E01–E12、Encrypt、原探针、图谱 yaml。
- 未 commit/push。

## 实现要点（供主控）

- v 路 CBD：`SamplePolyCbd2OneRowUb` 的 prfRow/outRow 分离（prf 第 3 行 → `ws[E0]`），避免 row=2 写 GM 越界（E12 FEEDBACK 同类教训）。
- u 路 L2 跳过 `DecompressMuAddHalfInPlace`；v 路保留。
- v 路 CBD：`SamplePolyCbd2OneRowUb` 的 prfRow/outRow 分离（prf 第 3 行 → `ws[E0]`），避免 row=2 写 GM 越界。
- u 路 L2 跳过 `DecompressMuAddHalfInPlace`；v 路保留。
- SIM TRACE：v 路 746/747、AIV 502/512 偶发丢失；验收以 744/745 + **402×3** + golden 为准（`TRACE.md`）。
