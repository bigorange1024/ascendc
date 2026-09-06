# TASK E11

## 元数据
- task_id: E11
- issued_at: 2026-09-06T08:40:00Z
- deadline_min: 40
- max_retries: 1
- silent_hang_min: 10
- abort_on: [timeout, no_progress, scope_breach, sim_hang]
- graph: docs/rg-encrypt-npu-hangfree.yaml
- related_nodes: [D-exp-e11, F-e10-byteencode-chain-sim-pass, J-sim-rewrite-blocks-ready]
- write_graph: no
- rules: graph_tests/SUBAGENT_RULES.md

## 目标
在 E10 壳上，于 **INTT 之后、Compress 之前**接入真 **Decompress_d（d=1，μ 消息嵌入）**；Host 提供 μ；再走 Compress→ByteEncode+SET(4)；≥3 轮 SIM 不挂。

## 白名单
- ONLY `graph_tests/toys/toy-e11-chain-plus-decompress-mu/`
- 可复制 E10（不改 E10）
- Decompress 只读参考：`ascendc-tests/ml-kem/ml-kem-1024/pass-f203-decompress-d-vec-k4/`（可拷贝自包含 `vendor/decompress_d/`）
- **d=1**（FIPS 消息嵌入）；Host 侧准备 μ（可用固定/随机 32B，须写清）
- 禁止改原探针 / Encrypt / E01–E10

## 验收
- [ ] 真 Decompress（非 TRACE stub）在 INTT 后、Compress 前
- [ ] 语义：`… → INTT → + Decompress₁(μ) → Compress → ByteEncode → SET(4)`
- [ ] 上游真链仍在；≥3 轮 `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` 不挂
- [ ] 尽量对拍（含 μ 路径短输出 / 整链 dst）
- [ ] `TRACE.md` / `STATUS.md` / `ORIGIN-decompress.md` / `_outbox/FEEDBACK-E11.md`（**support** `D-exp-e11`）
- [ ] ≤40min 墙钟；超时 ABORT 并写清阻塞

## 禁止
抄 Encrypt；复测 retracted；并行 SIM；改图谱 yaml；commit/push
