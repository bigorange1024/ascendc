# TASK-011

## 元数据
- task_id: TASK-011
- issued_at: 2026-09-03T17:15Z
- deadline_min: 35
- max_retries: 1
- silent_hang_min: 10
- abort_on: [timeout, no_progress, scope_breach, sim_hang]
- graph: docs/rg-kem-decrypt-hang.yaml
- related_nodes: [D-next-omit-set4-r2, J-omit-slot1-hangs-after-ntt, J-sim-empty-gm-spin-not-hang, F-decrypt-omit-set4-hangs-sim, Q-hang-which-layer]
- hypothesis_under_test: [J-omit-slot1-hangs-after-ntt]
- write_graph: no
- concurrency: solo

## 目标（一句话）
第一轮 GATE **合法 SET(4)** 并跑完 stub NTT 后，**第二轮 GATE 省略 SET(4)**：SIM 预期 124。默认路径仍绿。不要再用空 while 硬凑 SoftSync hang。

## 允许改动范围
- 白名单：`ascendc-tests/fix-decrypt-skel-mix-chain-toy/`、`graph_tests/_outbox/FB-TASK-011.md`、可选 STATUS / `graph_tests/INDEX.md`
- 禁止：stable/frozen；改 yaml；把 OMIT_SLOT0 空 while 改成 volatile 计数来硬凑 124；commit/push；并行 SIM

## 图谱摘录
| id | status | 要点 |
|----|--------|------|
| J-omit-slot1-hangs-after-ntt | unverified | 第二段 AIC Wait(4) 缺 SET(4) 应挂 |
| J-sim-empty-gm-spin-not-hang | verified | 空 while **不能**当 SIM hang 代理 |
| F-decrypt-omit-set4-hangs-sim | verified | 两轮都不 SET(4) 已 124；本单测「只第二轮」 |

## 步骤
1. 新开关 `SKEL_OMIT_SET4_R2` 默认 0。`run.sh` env→cmake。与 `SKEL_OMIT_SET4=1`、`SKEL_OMIT_SLOT0=1` **互斥**。
2. 语义：第一轮 `AivGateRound(slot0)` **仍 SET(4)**；第二轮 `AivGateRound(slot1)` **不 SET(4)**。AIC 第一段应能放行并完成第一轮 Cube，然后卡在第二段 `Wait(4)`。
3. 可选：删掉 `npu_lib.cmake` 里无用的 `SKEL_GATE/HEAVY/SKIPNTT/HOST_MU` 空宏（若存在）。
4. 串行 SIM：
   ```bash
   cd /workspace/ascendc-tests/fix-decrypt-skel-mix-chain-toy
   SKEL_OMIT_SET4_R2=0 SKEL_OMIT_SET4=0 SKEL_OMIT_SLOT0=0 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
   KERNEL_COMPUTE_BUDGET_SEC=60 SKEL_OMIT_SET4_R2=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
   ```
5. 默认绿 + R2 得 124 → support `J-omit-slot1-hangs-after-ntt`。R2 不挂 → weaken/refute，禁止硬凑。
6. STATUS + FEEDBACK。日志拷 `/opt/cursor/artifacts/decrypt-skel-toy-r2-A.log` 与 `decrypt-skel-toy-r2-B.log`。

禁止 commit/push。
