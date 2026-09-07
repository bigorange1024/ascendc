# FEEDBACK-E08

| 字段 | 值 |
|------|----|
| task_id | E08 |
| verdict | **PASS** |
| wall_clock_min | **~8**（issued 08:14Z → SIM 完成 ~08:22Z；deadline 40） |
| directory | `graph_tests/toys/toy-e08-shake-cbd-ntt-chain/` |
| hypothesis | `D-exp-e08` |

## 结果摘要

1. **默认 3 轮**：`SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` → **全绿**（不挂）。
2. Host TRACE：`100/101/105/110/111` 各 3。
3. L1 **真 SHAKE256** → **真 CBD(η=2)**：`200→210→211→212→220→221→203` ×3。
4. L2 **真 NTT** + **真 basemul** + **真 INTT** + SET(4) 仍在（与 E07 同号段）。
5. **CBD**：自包含拷贝 `vendor/cbd_eta2/`（只读参考 `pass-fix-f203-alg8-cbd-eta2-k4`）；短链单 poly `SamplePolyCbd2OneRowUb`；PRF@ws+P0 覆写 `src` 供 L2。
6. **golden 对拍**：
   - SHAKE：`diffs=0/32`
   - CBD：`diffs=0/256`
   - `dst` vs `golden`：`diffs=0/256`（`INTT(Alg.11(NTT(CBD(prf)),ĝ))`）
7. SoftSync：默认未加。kernel wall ≈ **86.9s**（budget 900）；Total tick **644679**。
8. CPU 孪生亦全绿（`bash run.sh -r cpu`）。

## 日志路径

| 跑次 | 路径 |
|------|------|
| CPU | `/opt/cursor/artifacts/e08-cpu.log` |
| 默认 3 轮 SIM | `/opt/cursor/artifacts/e08-default-sim.log` |
| 用例 tee | `graph_tests/toys/toy-e08-shake-cbd-ntt-chain/output/host_trace.log` |
| 文档 | 同目录 `TRACE.md`、`STATUS.md`、`ORIGIN-cbd.md` |

## 对图谱节点的 effect

| node | effect | note |
|------|--------|------|
| `D-exp-e08` | **support** | E07 壳 + 真 SHAKE+**真 CBD(η=2)**+NTT+basemul+INTT+SET(4) + ≥3 轮 SIM 不挂 + golden |
| `F-e07-intt-sim-pass` | cite | 2-launch / Host μ / 真 SHAKE+NTT+basemul+INTT+SET4 壳复用 |
| `D-use-blocks` | **support** | CBD η=2 积木自包含拷贝；未改原探针 |
| `D-short-experiments` | **honored** | 墙钟 ~8min ≪ 40 |
| `D-no-repeat-retracted` | **honored** | 未测双 Cube / GATE alone / OMIT_SET4 / SoftSync 复测 |

## 范围合规

- 仅改白名单 `toy-e08-shake-cbd-ntt-chain/` + 本 FEEDBACK outbox。
- 未改 E01–E07、Encrypt、原 CBD/Tag5T/ntt256 探针、图谱 yaml。
- 写 AscendC 前已读 ascendc-engineering-notes；API 均为已登记复用。
