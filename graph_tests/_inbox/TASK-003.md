# TASK-003

## 元数据
- task_id: TASK-003
- issued_at: 2026-09-03T10:14Z
- deadline_min: 45
- max_retries: 1
- silent_hang_min: 12
- abort_on: [timeout, no_progress, scope_breach, sim_hang]
- graph: docs/rg-kem-encrypt-hang.yaml
- related_nodes: [F-skel-gate-sim-pass, J-hang-needs-extra-factor, J-common-mix-flag13, Q-toy-repro, D-next-scale-mmad]
- hypothesis_under_test: [J-hang-needs-extra-factor, J-common-mix-flag13, Q-toy-repro]
- write_graph: no
- concurrency: solo

## 目标（一句话）
在已绿骨架上**放大/增多 Cube（flag 1/3）负荷**，SIM 观察是否挂死、超时或异常；检验「附加因子=更大 Cube 负荷」。

## 允许改动范围
- 白名单：`ascendc-tests/fix-encrypt-skel-mix-chain-toy/`、`graph_tests/_outbox/FB-TASK-003.md`、可选 STATUS/INDEX 一行
- 禁止：stable/frozen/真 SHAKE/SyncAll@Wait/自造 SoftSync/滥 launch/碎写 GM/commit/push/子 agent/并行 SIM
- **不要**再单测 GATE alone（已证伪）

## 必读
`TASK-003.md`、`FB-TASK-001.md`、`FB-TASK-002.md`、`CHARTER.md`、`SUBAGENT_RULES.md`、现有 `mmad_custom.cpp`

### 图谱摘录
| id | status | 要点 |
|----|--------|------|
| F-skel-gate-sim-pass | verified | stub+1/3+GATE SIM 绿 |
| J-gate-alone-sufficient-sim | retracted | GATE alone 不足 |
| J-hang-needs-extra-factor | unverified | 候选收缩到大 MMAD/真哈希/接缝等 |
| D-next-scale-mmad | active | 本单 |

## 步骤
1. 保留可切换：`SKEL_HEAVY=0` 接近现基线；`SKEL_HEAVY=1`（默认测）放大负荷。
2. 加压手段（择一或组合，须在注释写明选型）：
   - 增大单次 MMAD（如 16×64×64 或文档允许的更大合法 tiling），**和/或**
   - 连续 **≥4 轮** flag 1/3 Cube（NTT-like/INTT-like 各多轮），仍 1 MIX launch
3. 保持 stub 哈希；GATE 可维持默认开（`SKEL_GATE=1`）以免回退形态。
4. 验收仅 SIM：
   ```bash
   cd /workspace/ascendc-tests/fix-encrypt-skel-mix-chain-toy
   SKEL_HEAVY=1 SKEL_GATE=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
   # 对照：
   SKEL_HEAVY=0 SKEL_GATE=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
   ```
5. 若 HEAVY=1 **挂死/124**：记 support，交 FEEDBACK，停止。  
   若仍绿：记 weaken（大 MMAD alone 在 stub 下不足），交卷；**不要**再开第三档无限加压。
6. 写 `graph_tests/_outbox/FB-TASK-003.md`。

## 反馈要求
FEEDBACK 模板；图谱影响表必填。禁止 commit/push。
