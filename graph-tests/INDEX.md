# graph-tests — Encrypt cannbot 重建试验场

> **用途**：一刀一目录的积木拼装 / 同步探针；**不是** `examples/` 交付树。  
> **任务运营（任务书 / 反馈 / 日志）**：[`encrypt-rebuild-ops/`](encrypt-rebuild-ops/INDEX.md) · [`QUEUE.md`](encrypt-rebuild-ops/QUEUE.md)  
> **治理**：[`Encrypt-cannbot-rebuild-work-mode.md`](../docs/notes/Encrypt-cannbot-rebuild-work-mode.md)  
> **知识**：[`Encrypt-cannbot-rebuild-kb.md`](../docs/notes/Encrypt-cannbot-rebuild-kb.md) · DAG [`docs/rg-encrypt-cannbot-rebuild.yaml`](../docs/rg-encrypt-cannbot-rebuild.yaml)

## 状态（2026-09-08）

| 项 | 说明 |
|----|------|
| 任务书 | **T01–T09 + N01–N02 已预写**于 `encrypt-rebuild-ops/tasks/` |
| 派发 | **尚未派** Subagent（等下令） |
| NPU | Wave D = `wait_npu`（他 Agent 占用） |
| 实现目录 | 仍空；开刀后按 TASK 建 `toys/` / `bricks/` / `enc_related/` |

## 目录

| 路径 | 含义 |
|------|------|
| [`encrypt-rebuild-ops/`](encrypt-rebuild-ops/INDEX.md) | **任务/反馈/日志**（与实现分离） |
| [`toys/`](toys/INDEX.md) | Wave A 实现（RB-T01…） |
| [`bricks/`](bricks/INDEX.md) | Wave B 积木（RB-T04…） |
| [`enc_related/`](enc_related/INDEX.md) | Wave C 拼装（RB-T07…） |

## 禁令

- 禁止从 `pass-fix-f203-alg14*` / `alg20*` / `alg21*` / `*encrypt*` / `*encaps*` / `*decaps*` **抄实现**。  
- 禁止 subagent 改 KB / DAG yaml。
