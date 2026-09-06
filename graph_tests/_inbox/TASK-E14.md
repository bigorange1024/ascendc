# TASK E14

## 元数据
- task_id: E14
- issued_at: 2026-09-06T09:10:00Z
- deadline_min: 40
- max_retries: 1
- silent_hang_min: 10
- abort_on: [timeout, no_progress, scope_breach, sim_hang]
- graph: docs/rg-encrypt-npu-hangfree.yaml
- related_nodes: [D-exp-e14, F-e13-encrypt-shaped-glue-sim-pass, J-sim-rewrite-blocks-ready]
- write_graph: no
- rules: graph_tests/SUBAGENT_RULES.md

## 目标
在 E13 形态粘合壳上，换入真 **SampleNTT（Alg.7）** 生成 Â（k=2），替换 stub 右乘/公钥材料；保留 c1∥c2 + SET(4)；≥3 轮 SIM 不挂。

## 白名单
- ONLY `graph_tests/toys/toy-e14-glue-plus-samplentt/`
- 可复制 E13（不改 E13）
- SampleNTT 只读：`ascendc-tests/ml-kem/ml-kem-512/pass-fix-f203-alg7-sample-ntt-k2/`（可自包含 vendor）
- **k=2**；至少支撑 u 路所需 Â 列/行（完整 2×2 优先；墙钟紧可先 ≥2 个 SampleNTT poly 并文档说明）
- 禁止改原探针 / Encrypt / E01–E13

## 验收
- [ ] 真 SampleNTT（非 TRACE stub）参与 Â；L2 使用其结果而非全 stub 矩阵
- [ ] Encrypt 形态粘合仍在（L1 采样 / L2 代数压码；c 形输出）
- [ ] ≥3 轮 `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` 不挂
- [ ] 尽量 golden（SampleNTT 短输出与/或整链 c）
- [ ] `TRACE.md` / `STATUS.md` / `ORIGIN-samplentt.md` / `_outbox/FEEDBACK-E14.md`（**support** `D-exp-e14`）
- [ ] ≤40min；超时 ABORT 写清（勿死磕全矩阵）

## 禁止
抄 Encrypt；复测 retracted；并行 SIM；改图谱 yaml；commit/push
