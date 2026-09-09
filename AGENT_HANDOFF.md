# Agent 交接 — 每日刷新

> **最后刷新**：2026-09-09（`rg-encrypt-cann-ntt.yaml` 已迁 skill 骨架；`Q-ULT-NOHANG` 已答；NPU 仍停）  
> Git：本线文档可提交推送；`thirdparty/` 本身不进仓

## 依赖

| 项 | 路径 |
|----|------|
| 推理图谱 skill | `thirdparty/reasoning-graph-skill/SKILL.md`（Drive `16TwSu0…`；见 `SOURCE.md`） |
| 登记 | `docs/engineering/thirdparty-本地依赖.md` → Drive 手工包 |
| Encrypt×cann-ntt DAG | `docs/rg-encrypt-cann-ntt.yaml` |
| KB | `docs/notes/Encrypt-cann-ntt-kb.md` §7 |

积累/维护 `docs/rg-*.yaml`：**先读该 SKILL**；勿复制进 `.cursor/skills`。

## 真机 / 上轮结论

EN10–EN12 NPU **PASS-NOHANG**（详见 KB）；NPU 作业已停。

## 图谱状态（2026-09-09）

- `rg_validate` / `check_rg_dag`：**OK**（42 nodes）
- `Q-ULT-NOHANG`：**answered** ← `I-HOST-ORCH-NPU-NOHANG`
- 仍 **open**：`Q-CORRECTNESS-FULL`、`Q-OLD-L18-STILL-HANG`
- 渲染：`/opt/cursor/artifacts/rg-encrypt-cann-ntt.html`

## P0

无 NPU：按开放问或用户下一指令继续沉淀图谱 / 设计只读对照；换机须重下 Drive zip。
