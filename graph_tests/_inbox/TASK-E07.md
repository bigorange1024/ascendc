# TASK E07

## 元数据
- task_id: E07
- issued_at: 2026-09-06T08:00:00Z
- deadline_min: 40
- graph: docs/rg-encrypt-npu-hangfree.yaml
- related_nodes: [D-exp-e07, F-e06-shake-ntt-basemul-sim-pass, D-use-blocks]
- write_graph: no

## 目标
在 E06 壳上，于 basemul **之后**接入真 **INTT**；保留 SHAKE+NTT+basemul+SET(4)；≥3 轮 SIM 不挂。

## 白名单
- ONLY `graph_tests/toys/toy-e07-shake-ntt-basemul-intt/`
- 可复制 E06（不改 E06）
- INTT 只读参考：`ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-stage123-ntt-intt-polyvec8-vec/`（可拷贝自包含；禁止改原目录）
- 也可用与 E04 同系的 INTT（若 ntt256 有逆变换）；STATUS 写明语义是否 = Tag5T

## 验收
- [ ] 真 INTT（非 TRACE stub）接在 basemul 后
- [ ] ≥3 轮 SIM 不挂；尽量对拍
- [ ] TRACE.md / STATUS.md / `_outbox/FEEDBACK-E07.md`（support D-exp-e07）
- [ ] ≤40min

## 禁止
抄 Encrypt；改 E01–E06；复测 retracted
