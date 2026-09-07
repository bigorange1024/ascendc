# TASK E13

## 元数据
- task_id: E13
- issued_at: 2026-09-06T08:53:00Z
- deadline_min: 40
- max_retries: 1
- silent_hang_min: 10
- abort_on: [timeout, no_progress, scope_breach, sim_hang]
- graph: docs/rg-encrypt-npu-hangfree.yaml
- related_nodes: [D-exp-e13, F-e12-k2-multipoly-sim-pass, J-sim-rewrite-blocks-ready]
- write_graph: no
- rules: graph_tests/SUBAGENT_RULES.md

## 目标
在 E12 积木上做 **Encrypt 形态粘合**：L1≈采样角色、L2≈代数/压码角色；产出 **c 形打包**（玩具等价的 `c1||c2` 字节布局）；≥3 轮 SIM 不挂。

## 白名单
- ONLY `graph_tests/toys/toy-e13-encrypt-shaped-glue/`
- 可复制 E12（不改 E12）
- 公钥材料可用固定/stub（须 ORIGIN 写清）；**禁止抄现 Encrypt / Encaps 实现**
- 默认 **k=2**（控墙钟）；真积木复用自包含 vendor
- c 形：至少两段密文拼接（例如 u 路 ByteEncode ‖ v 路 ByteEncode）；布局在 STATUS/ORIGIN 冻结

## 验收
- [ ] Host 编排呈现 Encrypt 两段角色（L1 采样 / L2 代数+压码），非无意义改名
- [ ] 输出为 c 形打包（长度与布局文档化）；尽量 golden
- [ ] ≥3 轮 `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` 不挂
- [ ] `TRACE.md` / `STATUS.md` / `ORIGIN-glue.md` / `_outbox/FEEDBACK-E13.md`（**support** `D-exp-e13`）
- [ ] ≤40min；超时 ABORT

## 禁止
抄 Encrypt；复测 retracted；并行 SIM；改图谱 yaml；commit/push
