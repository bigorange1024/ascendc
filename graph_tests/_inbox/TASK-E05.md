# TASK E05

## 元数据
- task_id: E05
- issued_at: 2026-09-06T07:46:00Z
- deadline_min: 40
- max_retries: 1
- silent_hang_min: 10
- graph: docs/rg-encrypt-npu-hangfree.yaml
- related_nodes: [D-exp-e05, F-e04-skel-real-ntt-sim-pass, D-layer-real-compute, D-use-blocks]
- write_graph: no

## 目标
在 E04 壳上把 **L1 假采样**换成 **真 SHAKE256** 积木；保留 L2 真 NTT + SET(4)；≥3 轮 SIM 不挂。

## 白名单
- ONLY `graph_tests/toys/toy-e05-skel-shake-plus-ntt/`
- 可复制 E04 目录为起点；可引用/拷贝 `library/shared/shake_xof_kernel`、`pass-shake256-ascendc-toy` 所需文件进本目录
- **禁止改** E01–E04 原目录、shared 原文件（拷贝副本）、Encrypt、冻结用例

## 验收
- [ ] L1 走真 SHAKE（非空 TRACE stub）
- [ ] L2 仍为真 NTT + SET(4)
- [ ] ≥3 轮 SIM 不挂
- [ ] 尽量对拍 shake 短向量；来不及则 FEEDBACK 标明仅不挂
- [ ] TRACE.md / STATUS.md / `_outbox/FEEDBACK-E05.md`（support D-exp-e05）
- [ ] ≤40min

## 禁止
抄 Encrypt；复测 retracted；并行 SIM；改图谱

## 必读
ascendc-engineering-notes SKILL；shake256 STATUS；FEEDBACK-E04
