# TASK E04

## 元数据
- task_id: E04
- issued_at: 2026-09-06T07:40:00Z
- deadline_min: 40
- max_retries: 1
- silent_hang_min: 10
- graph: docs/rg-encrypt-npu-hangfree.yaml
- related_nodes: [D-exp-e04, F-e03-stage-skel-sim-pass, D-layer-real-compute, D-use-blocks]
- write_graph: no

## 目标
在 **E03 形态骨架**上，把 L2 **假 NTT** 换成真单 poly NTT 积木（只读参考 `ascendc-tests/pass-merged-kyber-mix-ntt256/`），保留 SET(4) 与 2-launch；≥3 轮 SIM 不挂。

## 白名单
- ONLY `graph_tests/toys/toy-e04-skel-plus-real-ntt/`
- 可复制 E03 壳；可复制 ntt256 **所需源文件进本目录**（自包含），禁止改 E03/ntt256 原目录/Encrypt
- 注明：本积木 golden ≠ F203 Tag5T（STATUS 写清）

## 验收
- [ ] ≥3 轮 SIM 跑完（不挂）
- [ ] L2 确有真 NTT 计算路径（非空 TRACE stub）
- [ ] 尽量对拍 ntt256 风格 golden；来不及则 magic+不挂并 FEEDBACK 标明
- [ ] TRACE.md / STATUS.md / `_outbox/FEEDBACK-E04.md`（support D-exp-e04）
- [ ] ≤40min

## 禁止
抄 Encrypt；复测 retracted；改冻结目录；并行 SIM

## 必读
ascendc-engineering-notes SKILL；ntt256 STATUS；FEEDBACK-E03
