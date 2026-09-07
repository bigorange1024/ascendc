# TASK-008（草稿；TASK-007 交卷并释放 SIM 后 issued）

## 元数据
- task_id: TASK-008
- issued_at: TBD
- deadline_min: 90
- max_retries: 1
- silent_hang_min: 15
- abort_on: [timeout, no_progress, scope_breach, sim_hang]
- graph: docs/rg-kem-decrypt-k131.yaml
- related_nodes: [D-next-reverify-sim, Q-sim-repro-now, D-diag-k-vs-jzc, Q-k-equals-jzc, F-force-rebuild-discipline]
- hypothesis_under_test: [D-next-reverify-sim, J-k131-reject-path]
- write_graph: no
- concurrency: solo

## 目标（一句话）
按 `graph_tests/DECRYPT_K131_PLAN.md` **Step-0**：FORCE 复验默认 2-launch Decaps **SIM**；日志确认 `chain_ntt`/`prep_ntt`；并做 **K vs J(z‖c) vs accept** 诊断（即使 SIM 绿也算一遍，便于对照用户 NPU）。

## 允许改动
- 白名单：
  - `graph_tests/_outbox/FB-TASK-008.md`
  - 可选：`graph_tests/` 下只读诊断脚本草稿（若需，放 `graph_tests/decrypt_diag_k_vs_jzc.py`，**勿**改 stable verify 默认行为）
  - 可选：`examples/stable/.../stable-fips203-mlkem-kem-decaps-k4/` **仅**若必须加**调试 env 门控**的 dump（默认关闭）；优先无码改只跑现有 run.sh
- 禁止：改 Encaps/clean toy、frozen、commit/push、并行 SIM、子 agent

## 步骤
1. 读 `DECRYPT_K131_PLAN.md` §3 Step-0 / §2 H1。
2. 跑：
   ```bash
   cd /workspace/examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-decaps-k4
   KEM_DECAPS_FORCE_REBUILD=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
   ```
3. 确认日志含 `chain_ntt` 与 `prep_ntt`（或文档等价 launch 文案）。
4. 记录 `K.bin max_abs_diff`。
5. 诊断：用 `input/dk.bin`（或 gen 布局）取 `z`，`input/c.bin`，算 `J(z‖c)`，与 `output/K.bin`、`golden/K.bin` 比（回答 Q-k-equals-jzc；SIM 绿时预期 K==accept≠J）。
6. **仅当默认 SIM 红**：再依次 A → B → C（每次等前一次结束），写入分段结果。
7. `FB-TASK-008.md`（图谱影响表）。

## 反馈要求
FEEDBACK 模板；禁止 commit/push。
