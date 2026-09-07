# TASK-010

## 元数据
- task_id: TASK-010
- issued_at: 2026-09-03T17:05Z
- deadline_min: 35
- max_retries: 1
- silent_hang_min: 10
- abort_on: [timeout, no_progress, scope_breach, sim_hang]
- graph: docs/rg-kem-decrypt-hang.yaml
- related_nodes: [J-omit-slot0-spin-hangs, F-softsync-two-slots, F-decrypt-skel-legal-sim-pass, D-next-omit-slot0, Q-hang-which-layer, Q-toy-repro]
- hypothesis_under_test: [J-omit-slot0-spin-hangs]
- write_graph: no
- concurrency: solo

## 目标（一句话）
在已绿的 Decrypt 握手 toy 上增加 **`SKEL_OMIT_SLOT0`**：AIV0 不写 slot0 哨兵 → AIV1 自旋无法 SET(4) → SIM 预期 124；默认路径仍须绿。

## 允许改动范围
- 白名单：
  - `ascendc-tests/fix-decrypt-skel-mix-chain-toy/`（CMake/`run.sh`/kernel/`STATUS.md`/`scripts/verify` 若需读 env）
  - `graph_tests/_outbox/FB-TASK-010.md`
  - 可选 `graph_tests/INDEX.md` 一行
- 禁止：stable/frozen；改图谱 yaml；OMIT_SET4 与 OMIT_SLOT0 同时为 1；commit/push；子 agent；并行 SIM；SyncAll@Wait；双向 SoftSync

## 必读
1. `graph_tests/_inbox/TASK-010.md`（本文件）、`DECRYPT_HANG_PLAN.md` T2
2. 现有 `mmad_custom.cpp` SoftSyncArrive / AivGateRound
3. 图谱摘录下表

| id | kind | status | statement |
|----|------|--------|-----------|
| J-omit-slot0-spin-hangs | inference | unverified | AIV0 不 Arrive(slot0) ⇒ AIV1 自旋且 AIC Wait(4) |
| F-decrypt-skel-legal-sim-pass | fact | verified | 默认路径 wall 3.9s 绿 |
| F-decrypt-omit-set4-hangs-sim | fact | verified | 已证缺 SET(4)⇒124（本单不要再测 OMIT_SET4） |
| D-next-omit-slot0 | decision | active | 本单要做的事 |

## 步骤
1. 增加编译开关 `SKEL_OMIT_SLOT0`（默认 **0**），`run.sh` 从 env 传入 cmake `-D`（照现有 OMIT_SET4 接法）。
   - 与 `SKEL_OMIT_SET4=1` **互斥**（run.sh 两者同时为 1 则报错退出）。
2. 语义（写进注释与 STATUS）：
   - `SKEL_OMIT_SLOT0=1` 且 `subBlockID==0`：**不写** `s[0]=1`（Arrive 对 AIV0 变成空操作）。
   - AIV1 **仍** `while (s[0]==0)`（应永远转）。
   - AIV0 **仍可**走后续 `SET(4)`（模拟「AIV0 做完 prep 但忘了写哨兵」）；不要顺便把 OMIT_SET4 打开。
3. 串行 SIM（先回归默认，再故障注入）：
   ```bash
   cd /workspace/ascendc-tests/fix-decrypt-skel-mix-chain-toy
   SKEL_OMIT_SLOT0=0 SKEL_OMIT_SET4=0 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
   KERNEL_COMPUTE_BUDGET_SEC=60 SKEL_OMIT_SLOT0=1 SKEL_OMIT_SET4=0 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
   ```
4. 判读：
   - 默认仍绿 + OMIT_SLOT0 得 **124** → support `J-omit-slot0-spin-hangs`
   - 默认挂 → 实现错误，max_retries=1 修一次
   - OMIT_SLOT0 **不挂** → refute/weaken，交卷，禁止改参硬凑
5. 更新 STATUS；写 `graph_tests/_outbox/FB-TASK-010.md`。

## 反馈要求
FEEDBACK 模板；两档结果表；图谱影响表必填 `J-omit-slot0-spin-hangs`。禁止 commit/push。
