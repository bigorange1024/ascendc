# TASK E15

## 元数据
- task_id: E15
- issued_at: 2026-09-06T10:52:00Z
- deadline_min: 40
- max_retries: 1
- silent_hang_min: 10
- abort_on: [timeout, no_progress, scope_breach, sim_hang]
- graph: docs/rg-encrypt-npu-hangfree.yaml
- related_nodes: [D-exp-e15, F-e14-samplentt-urow-sim-pass, F-samplentt-needs-own-launch]
- write_graph: no
- rules: graph_tests/SUBAGENT_RULES.md

## 目标
在 E14 壳上补齐 **Â 的 (1,0)(1,1)** 真 SampleNTT，形成完整 **2×2**；保持 **独立 SampleNTT launch**（勿嵌 MIX）；保留粘合 + c1∥c2 + SET(4)；≥3 轮 SIM 不挂。

## 白名单
- ONLY `graph_tests/toys/toy-e15-samplentt-a-full-2x2/`
- 可复制 E14（不改 E14）
- SampleNTT vendor 复用 E14 自包含拷贝
- **硬约束**：SampleNTT 必须独立 launch/phase（见 `F-samplentt-needs-own-launch`）
- 禁止改原探针 / Encrypt / E01–E14

## 验收
- [ ] Â 四个元均真 SampleNTT（非 stub）
- [ ] 粘合形态仍在；≥3 轮 `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` 不挂
- [ ] 尽量 golden（SampleNTT 与/或整链 c）
- [ ] `TRACE.md` / `STATUS.md` / `ORIGIN-a-2x2.md` / `_outbox/FEEDBACK-E15.md`（**support** `D-exp-e15`）
- [ ] ≤40min；接近 35min 未齐 → **ABORT** 写清已完成几元

## 禁止
嵌 MIX 同 kernel 跑 SampleNTT；抄 Encrypt；复测 retracted；并行 SIM；改图谱；commit/push
