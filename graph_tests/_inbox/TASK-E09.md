# TASK E09

## 元数据
- task_id: E09
- issued_at: 2026-09-06T08:24:00Z
- deadline_min: 40
- graph: docs/rg-encrypt-npu-hangfree.yaml
- related_nodes: [D-exp-e09, F-e08-cbd-chain-sim-pass, J-sim-rewrite-blocks-ready]
- write_graph: no

## 目标
在 E08 壳上，于 **INTT 之后**接入真 **Compress**；保留 SHAKE→CBD→NTT→basemul→INTT+SET(4)；≥3 轮 SIM 不挂。

## 白名单
- ONLY `graph_tests/toys/toy-e09-chain-plus-compress/`
- 可复制 E08（不改 E08）
- Compress 只读参考：`ascendc-tests/ml-kem/ml-kem-1024/pass-f203-compress-d-vec-k4/` 或 `pass-f203-compress-unified-int-vec-k4/`（可拷贝自包含）
- 禁止改原探针 / Encrypt / E01–E08

## 验收
- [ ] 真 Compress（非 TRACE stub）在 INTT 后
- [ ] 上游真链仍在；≥3 轮 SIM 不挂
- [ ] 尽量对拍 compress 短输出
- [ ] TRACE.md / STATUS.md / `_outbox/FEEDBACK-E09.md`（support D-exp-e09）
- [ ] ≤40min

## 禁止
抄 Encrypt；复测 retracted；并行 SIM
