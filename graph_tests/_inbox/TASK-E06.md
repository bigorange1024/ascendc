# TASK E06

## 元数据
- task_id: E06
- issued_at: 2026-09-06T07:53:00Z
- deadline_min: 40
- max_retries: 1
- silent_hang_min: 10
- graph: docs/rg-encrypt-npu-hangfree.yaml
- related_nodes: [D-exp-e06, F-e05-shake-ntt-sim-pass, D-layer-real-compute, D-use-blocks]
- write_graph: no

## 目标
在 E05 壳上，于 L2 **NTT 之后**接入真 **basemul / MultiplyNTTs** 积木；保留 SHAKE + SET(4)；≥3 轮 SIM 不挂。

## 白名单
- ONLY `graph_tests/toys/toy-e06-shake-ntt-basemul/`
- 可复制 E05 为起点（不改 E05）
- 积木只读参考：`ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg11-12-multiplyntts-k4/`（或 `innerproduct-k4`）；可拷贝进本目录自包含
- 禁止改原探针 / Encrypt / E01–E05

## 验收
- [ ] L1 真 SHAKE、L2 真 NTT、**真 basemul**（非 TRACE stub）
- [ ] SET(4) 仍在；≥3 轮 SIM 不挂
- [ ] 尽量对拍 multiplyntts/innerproduct 短输出；来不及则 FEEDBACK 标明仅不挂
- [ ] TRACE.md / STATUS.md / `_outbox/FEEDBACK-E06.md`（support D-exp-e06）
- [ ] ≤40min

## 禁止
抄 Encrypt；复测 retracted；并行 SIM

## 必读
ascendc-engineering-notes SKILL；multiplyntts STATUS；FEEDBACK-E05
