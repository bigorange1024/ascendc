# TASK-004

## 元数据
- task_id: TASK-004
- issued_at: 2026-09-03T10:21Z
- deadline_min: 40
- max_retries: 1
- silent_hang_min: 10
- abort_on: [timeout, no_progress, scope_breach, sim_hang]
- graph: docs/rg-kem-encrypt-hang.yaml
- related_nodes: [J-empty-trace-aic-wait4, J-hang-needs-extra-factor, F-trace-empty-0-16, F-gate-4-8, D-next-skipntt-wait4, Q-toy-repro]
- hypothesis_under_test: [J-empty-trace-aic-wait4, Q-toy-repro]
- write_graph: no
- concurrency: solo

## 目标（一句话）
在骨架上实现 **skipNtt 入口 AIC Wait(4)** 拓扑；正常路径应 SIM 绿；**故意不 SET(4)** 对照应 SIM 超时挂死——检验空 TRACE / WAIT(4) 死等模型。

## 允许改动范围
- 白名单：`ascendc-tests/fix-encrypt-skel-mix-chain-toy/`、`graph_tests/_outbox/FB-TASK-004.md`、可选 STATUS/INDEX
- 禁止：stable/frozen/真 SHAKE/SyncAll@Wait/自造 SoftSync/滥 launch/commit/push/子 agent/并行 SIM
- 本单允许**受控故障注入**（省略 SET4）作对照，须用 env 开关，默认不要省略

## 步骤
1. 增加模式（名称可微调，须写进 STATUS）：
   - `SKEL_SKIPNTT=1`：AIC **入口即** `Wait(4)`（对齐 l18 skipNtt）；AIV 先做 stub（可短），再 `Set(4)`，随后既有 1/3 等可简化或保留。
   - `SKEL_OMIT_SET4=1`：AIV **不** `Set(4)`（故障注入）。仅与 SKIPNTT=1 联用。
2. 跑三档 SIM（**串行**，勿并行）：
   ```bash
   cd /workspace/ascendc-tests/fix-encrypt-skel-mix-chain-toy
   # A 基线（可沿用现默认）
   SKEL_SKIPNTT=0 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
   # B skipNtt 正常
   SKEL_SKIPNTT=1 SKEL_OMIT_SET4=0 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
   # C 故障注入：预期超时 124 或明显挂死
   SKEL_SKIPNTT=1 SKEL_OMIT_SET4=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
   ```
3. 预算：C 档若挂，**等 timeout 一次即可**（依赖用例 `KERNEL_COMPUTE_BUDGET_SEC`，可临时降到 60 以省时间，须在 FEEDBACK 写明）；不要杀进程后无记录。
4. 判读写入 FEEDBACK：
   - B 绿 + C 挂 → **support** `J-empty-trace-aic-wait4`（机制可在 SIM 复现）
   - B 挂 → 记录，可能实现错误（max_retries=1 修一次）
   - C 不挂 → **weaken/refute** 该 SIM 模型，交卷
5. 写 `graph_tests/_outbox/FB-TASK-004.md`。

## 反馈要求
FEEDBACK 模板；三档结果表；图谱影响表。禁止 commit/push。
