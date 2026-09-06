# TASK E02

## 元数据
- task_id: E02
- issued_at: 2026-09-06T07:27:00Z
- deadline_min: 30
- max_retries: 1
- silent_hang_min: 8
- graph: docs/rg-encrypt-npu-hangfree.yaml
- related_nodes: [D-exp-e02, F-e01-8round-sim-pass, D-softsync-follow, D-set4-invariant, D-short-experiments]
- hypothesis_under_test: [D-exp-e02]
- write_graph: no
- concurrency: solo

## 目标（一句话）
新目录：双 AIV **先 SoftSyncArrive 再 SET(4)**，AIC Wait(4)；默认 SIM ≥3 轮绿。

## 允许改动
- 白名单：**仅** `graph_tests/toys/toy-e02-softsync-then-set4/`
- 只读参考：`toy-e01-2launch-set4-trace-repeat/`（工程壳）、`ascendc-tests/fix-decrypt-skel-mix-chain-toy/` 的 `SoftSyncArrive` 定式
- 禁止：改 E01/冻结目录/stable/图谱/知识库；禁止自造双向 SoftSync；禁止复测双 Cube/GATE alone

## 必读
1. `graph_tests/SUBAGENT_RULES.md`
2. FEEDBACK-E01（已 PASS，可复用壳思路）
3. decrypt skel 中 SoftSyncArrive 注释（单向 AIV0 写哨兵 / AIV1 自旋）
4. `.cursor/skills/ascendc-engineering-notes/SKILL.md`

## 步骤
1. 新建白名单目录；可从 E01 **复制工程壳后改 kernel**（不要改 E01 原目录）。
2. L2：双 AIV SoftSyncArrive → 再各自 SET(4)；AIC Wait(4)；数字 TRACE（沿用 E01 号段，可增加 SoftSync 前/后号如 503/513）。
3. Host：≥3 轮 2-launch（或单 launch 若你证明够；优先与 E01 同 2-launch）全完成。
4. **可选对照**（时间够再做）：`OMIT_SOFTSYNC=1` 仍绿 → FEEDBACK 写「本骨架 SoftSync 非必要」（weaken，非 FAIL）。
5. 写 TRACE.md、STATUS.md、`graph_tests/_outbox/FEEDBACK-E02.md`。

## 验收 PASS
- [ ] 仅白名单改动
- [ ] 默认 SIM ≥3 轮绿；有 SoftSync→SET4 顺序
- [ ] TRACE 仅数字 + TRACE.md
- [ ] FEEDBACK 含 effect（support D-exp-e02）
- [ ] ≤30min；超时 TIMEOUT 交卷

## 不做
- OMIT_SET4 发现实验（已知）
- 真 NTT/哈希/Encrypt 业务
