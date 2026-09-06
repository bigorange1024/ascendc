# TASK E10

## 元数据
- task_id: E10
- issued_at: 2026-09-06T08:33:00Z
- deadline_min: 40
- max_retries: 1
- silent_hang_min: 10
- abort_on: [timeout, no_progress, scope_breach, sim_hang]
- graph: docs/rg-encrypt-npu-hangfree.yaml
- related_nodes: [D-exp-e10, F-e09-compress-chain-sim-pass, J-sim-rewrite-blocks-ready]
- write_graph: no
- rules: graph_tests/SUBAGENT_RULES.md

## 目标
在 E09 壳上，于 **Compress 之后**接入真 **ByteEncode_d（d=4）**；保留 SHAKE→CBD→NTT→basemul→INTT→Compress+SET(4)；≥3 轮 SIM 不挂。

## 白名单
- ONLY `graph_tests/toys/toy-e10-chain-plus-byteencode/`
- 可复制 E09（不改 E09）
- ByteEncode 只读参考：`ascendc-tests/ml-kem/ml-kem-1024/pass-f203-byteencode-d-vec-k4/`（可拷贝自包含 `vendor/byteencode_d/`）
- **d=4**（与 E09 Compress_d=4 对齐；单 poly 编码输出 **128B**）
- 禁止改原探针 / Encrypt / `pass-fix-f203-alg14-*-encrypt-pack*` / E01–E09

## 验收
- [ ] 真 ByteEncode_d（非 TRACE stub）在 Compress 后、SET(4) 前（或紧邻可论证的安全点）
- [ ] 上游真链仍在；≥3 轮 `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` 不挂（与 E09 同入口）
- [ ] 尽量对拍 encode 短输出（128B）及/或整链 dst
- [ ] `TRACE.md` / `STATUS.md` / `_outbox/FEEDBACK-E10.md`（**support** `D-exp-e10`）
- [ ] ≤40min 墙钟；超时 ABORT 并写清阻塞

## 禁止
抄 Encrypt / encrypt-pack 整图；复测 retracted（双 Cube / GATE alone / OMIT_SET4 / SoftSync 复测）；并行 SIM；改图谱 yaml；commit/push
