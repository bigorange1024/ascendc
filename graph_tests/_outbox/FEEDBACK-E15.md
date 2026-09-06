# FEEDBACK-E15

| 字段 | 值 |
|------|----|
| task_id | E15 |
| verdict | **PASS** |
| wall_clock_min | **~38**（SIM 3 轮 kernel ~429s 含编译；deadline 40） |
| directory | `graph_tests/toys/toy-e15-samplentt-a-full-2x2/` |
| hypothesis | `D-exp-e15` |

## 结果摘要

1. **3 轮 SIM**：`SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` → **全绿**（rc=0，不挂）。
2. **完整 2×2 真 SampleNTT**：独立 launch（phase=1）；设备 `BuildAlg7SampleNttFromSeedD(SEED_D,(0,0)…(1,1))` → ws[G0…G3]；TRACE **300/302/303/304/305 ×3**。
3. **Encrypt 粘合保留**：L1 SHAKE+CBD(u×2+v) → SampleNTT×4 → L2 u/v 真链 + **c1∥c2(384B)** + SET(4)；Host **100/101/102/104/106/105/110/111 ×3**。
4. **golden**：SHAKE 0/32；CBD(u) 0/512；**c 0/384B**（四元 SampleNTT oracle；v 路 G2=(1,0) 非 stub）。
5. SIM kernel wall ≈ **429s**（budget 1800）；402×3；用例根无 stray dump。
6. **CPU ×1**（`TOY_ROUNDS=1`）：golden 0/384B PASS（全 3 轮 CPU 极慢未跑）。

## 矩阵覆盖

| 元 | 设备 SampleNTT | L2 消费 |
|----|----------------|---------|
| (0,0) | ✅ G0 | u0 basemul |
| (0,1) | ✅ G1 | u1 basemul |
| (1,0) | ✅ G2 | v basemul |
| (1,1) | ✅ G3 | 完整性（未进 basemul 链） |

## 相对 E14 最小增量

- `L2RunSampleNttSubBlock0`：+2 次 `BuildAlg7SampleNttFromSeedD(1,0)/(1,1)` → G2/G3；TRACE +304/305。
- `tiling.h`：G3 偏移；`kGBytes=k×k=4` poly；Minv0 后移。
- `gen_data.py`：四元 oracle；v 路 `g10=(1,0)`；移除 Host g.bin 依赖。
- `main.cpp`：不再 H2D `g.bin`（全设备生成）。

## 日志路径

| 跑次 | 路径 |
|------|------|
| SIM 3 轮（验收） | `/opt/cursor/artifacts/e15-sim.log` |
| CPU 1 轮 | `/opt/cursor/artifacts/e15-cpu-r1.log` |
| 用例 tee | `graph_tests/toys/toy-e15-samplentt-a-full-2x2/output/host_trace.log` |
| 文档 | `TRACE.md` / `STATUS.md` / `ORIGIN-a-2x2.md` |

## 对图谱节点的 effect

| node | effect | note |
|------|--------|------|
| `D-exp-e15` | **support** | 完整 2×2 SampleNTT + ≥3 轮 SIM 不挂 + golden |
| `F-e14-samplentt-urow-sim-pass` | cite | E14 壳来源 |
| `F-samplentt-needs-own-launch` | **support** | 仍独立 phase；4×SampleNTT 未嵌 MIX 真链 |
| `J-sim-rewrite-blocks-ready` | **support** | L2 积木 + 四元 SampleNTT 仍绿 |
| `D-use-blocks` | **support** | 仅新 toy + vendor/sample_ntt |
| `D-short-experiments` | **honored** | 墙钟 ~38min ≤40 |
| `D-no-copy-encrypt` | **honored** | 未抄 Encrypt |
| `D-no-repeat-retracted` | **honored** | 未测 retracted |

## 实现要点

- **3-launch Host**：L1 → **SampleNTT phase**（AIV subBlock0，4×串行）→ μ → L2。
- v 路 basemul 改读 **G2=(1,0)** 真 â；G3=(1,1) 采样完整性（玩具语义仍非完整 A^T·r）。
- 未改 E01–E14 / Encrypt / 原探针 / 图谱 yaml。

## 范围合规

- 仅改白名单 `toy-e15-samplentt-a-full-2x2/` + 本 FEEDBACK。
- 未 commit/push。
