# TASK E12

## 元数据
- task_id: E12
- issued_at: 2026-09-06T08:45:00Z
- deadline_min: 40
- max_retries: 1
- silent_hang_min: 10
- abort_on: [timeout, no_progress, scope_breach, sim_hang]
- graph: docs/rg-encrypt-npu-hangfree.yaml
- related_nodes: [D-exp-e12, F-e11-decompress-mu-sim-pass, J-sim-rewrite-blocks-ready]
- write_graph: no
- rules: graph_tests/SUBAGENT_RULES.md

## 目标
在 E11 壳上将几何扩到 **k=2（两路 poly）**；共享 **2-launch + SET(4)**；尽量复用已接真积木；≥3 轮 SIM 不挂。

## 白名单
- ONLY `graph_tests/toys/toy-e12-chain-k2-multipoly/`
- 可复制 E11（不改 E11）
- 积木只读复用 E11 已自包含的 vendor（可再拷贝）；禁止改原探针 / Encrypt / E01–E11
- **k=2**：两路 poly 各走真链语义（允许共享 SHAKE/μ 输入策略，但须在 ORIGIN/STATUS 写清）
- 输出几何与 golden 须与 k=2 对齐（例如 2×128B ByteEncode 或文档约定的打包）

## 验收
- [ ] k=2 真几何（非仅 TRACE 翻倍 stub）
- [ ] 上游真积木仍在（可含 Decompress(μ)；若为控时省略 μ，须在 FEEDBACK 标明并仍保留 Compress→ByteEncode）
- [ ] ≥3 轮 `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` 不挂
- [ ] 尽量 golden 对拍
- [ ] `TRACE.md` / `STATUS.md` / `ORIGIN-k2.md` / `_outbox/FEEDBACK-E12.md`（**support** `D-exp-e12`）
- [ ] ≤40min；超时 ABORT 写清阻塞（勿死磕）

## 禁止
抄 Encrypt；复测 retracted；并行 SIM；改图谱 yaml；commit/push
