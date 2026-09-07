# Encrypt × cannbot-skills · 直调开发绑定（2026-09-07）

> **用户目标（已口述）**：充分使用 `thirdparty/cannbot-skills`，按昇腾 **Ascend C 直调** 路径，**从头**做好 Encrypt 算子开发。  
> **本仓硬约束仍在**：不抄旧 Encrypt；失败优先；SIM 穷尽再上机；910B3 机时稀缺、未经明示不连。  
> **工作区过程目录**：`.cannbot/mlkem-pke-encrypt/`（cannbot 约定中间产物）。

---

## 1. 怎么「充分使用」（Cursor 适配）

cannbot 官方 `ops-direct-invoke` 假定 OpenCode/Claude 多角色 Plugin（PM 禁止写码）。本仓在 **Cursor 单 Agent + Task subagent** 下执行等价流程：

| cannbot 角色 | 本仓谁干 |
|--------------|----------|
| PM 编排 | 主控本会话 |
| architect / developer-* / QA | 主控按 skill 写交付件，或派 Task subagent；**强制加载对应 cannbot skill** |
| `.cannbot/<op>/` | 需求/方案/state/问卷落盘处 |

**禁止**：跑会改写根 `AGENTS.md` / 全仓接管的完整 `init.sh`（除非用户当次明确授权）。  
**允许**：只读引用 `thirdparty/cannbot-skills/**`；在 `.cannbot/` 下落过程文件；用其 `scripts/`（sync_audit、env-check、plog 等）。

若要把 skill **链入** `.cursor/skills/vendor/`，须用户当次确认（Rule「Rule/Skill 变更」）。

---

## 2. 阶段 ↔ cannbot Skills（Encrypt 专用）

| 阶段 | 交付 | 必载 Skills |
|------|------|-------------|
| 0 环境 | `.cannbot/环境信息.md` | `ascendc-env-check`、`npu-arch`、`cann-env-setup`（仅文档） |
| 1 需求 CP1 | `01-requirement.md` | `ascendc-docs-gen`（需求分析模板）、`npu-arch` |
| 2.1 测试方案 | `02-test-plan.md` | `ascendc-st-design`、`ops-precision-standard`、`ascendc-whitebox-design` |
| 2.2 开发方案 CP2.2 | `03-design.md` | `ascendc-docs-gen`（详细设计）、`ascendc-tiling-design`、`ascendc-api-best-practices`（含 **api-crosscore-sync**）、本仓 KB+DAG |
| 3 编码/联调 | 新目录源码 | `ascendc-direct-invoke-template`（壳）、`ascendc-api-best-practices`、`ascendc-sync-audit`（每刀必跑）、本仓 `ascendc-engineering-notes` |
| 卡死/挂 | 分诊报告 | `ascendc-crash-debug`、`ascendc-sync-audit/deadlock-triage`、`ascendc-runtime-debug` |
| 真机故障 | dump/plog | `msaicerr-toolkit`、`msnpureport-toolkit`、`asys-toolkit` |
| CP3 精度 | 对拍报告 | `ascendc-precision-debug`、`ops-precision-standard` |
| CP4 性能 | 可选后置 | `ops-profiling`、`ops-simulator`、`ascendc-perf-optimize`（**卡死未清前不启**） |
| CP5 检视 | 检视单 | `ascendc-code-review`、`ascendc-sync-audit` 复跑 |

**明确不用作主路径**：Catlass / PyPTO / TileLang / Triton / torch.compile / model-infer-*（编程模型或目标不符）。

---

## 3. 与旧「卡死重写」计划的关系（2026-09-07 用户锁定）

| 项 | 决定 |
|----|------|
| 核心 | **仍只攻卡死**；正确性延后 |
| 方法 | **图谱实验**（`graph-tests/`）+ **主动用** cannbot skills |
| 落点 | toys 已穷尽 → **`enc_related/`**（ER01 起） |
| Skills | 读/跑 `thirdparty/cannbot-skills`；**不**链进 `.cursor/skills` |
| 旧 Encrypt | 只读；可 sync_audit 对照；**禁抄核** |
| 上机 | 910B3 未经明示不连 |

---

## 4. 当前进度

| 步 | 状态 |
|----|------|
| 用户锁定 | `.cannbot/mlkem-pke-encrypt/CP1-用户结论.md` |
| 旧核 sync_audit（只读） | `.cannbot/mlkem-pke-encrypt/tmp/sync_audit_l18_l19.json` |
| ER01 任务书 | `graph-tests/enc_related/ER01-TASK.md` |
| ER01 编码 | 进行中 / 待 subagent |
