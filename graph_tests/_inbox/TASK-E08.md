# TASK E08

## 元数据
- task_id: E08
- issued_at: 2026-09-06T08:14:00Z
- deadline_min: 40
- graph: docs/rg-encrypt-npu-hangfree.yaml
- related_nodes: [D-exp-e08, F-e07-intt-sim-pass, D-use-blocks]
- write_graph: no

## 目标
在 E07 壳上，于 **SHAKE 之后**接入真 **CBD(η=2)**，再进入 NTT→basemul→INTT+SET(4)；≥3 轮 SIM 不挂。

## 白名单
- ONLY `graph_tests/toys/toy-e08-shake-cbd-ntt-chain/`
- 可复制 E07（不改 E07）
- CBD 只读参考：`ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg8-cbd-eta2-k4/`（可拷贝自包含）
- 禁止改原探针 / Encrypt / E01–E07

## 验收
- [ ] 真 CBD（非 TRACE stub）接在 SHAKE 后、NTT 前
- [ ] 后续真 NTT/basemul/INTT + SET(4) 仍在
- [ ] ≥3 轮 SIM 不挂；尽量对拍 CBD 或短链
- [ ] TRACE.md / STATUS.md / `_outbox/FEEDBACK-E08.md`（support D-exp-e08）
- [ ] ≤40min

## 禁止
抄 Encrypt；复测 retracted；并行 SIM
