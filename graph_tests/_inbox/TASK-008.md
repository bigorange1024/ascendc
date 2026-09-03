# TASK-008

## 元数据
- task_id: TASK-008
- issued_at: 2026-09-03T11:20Z
- deadline_min: 90
- max_retries: 1
- silent_hang_min: 15
- abort_on: [timeout, no_progress, scope_breach, sim_hang]
- graph: docs/rg-kem-decrypt-k131.yaml
- related_nodes: [D-next-reverify-sim, Q-sim-repro-now, D-diag-k-vs-jzc, Q-k-equals-jzc, F-force-rebuild-discipline]
- hypothesis_under_test: [D-next-reverify-sim, J-k131-reject-path]
- write_graph: no
- concurrency: solo
- note: 由主控执行（重复 TASK-007 子任务卡住期间不另开 subagent）

## 目标
按 `graph_tests/DECRYPT_K131_PLAN.md` Step-0：FORCE 复验默认 2-launch Decaps SIM；日志确认路径；K vs J(z‖c) vs accept 诊断。

## 步骤见 draft；本文件为 issued 标记。
