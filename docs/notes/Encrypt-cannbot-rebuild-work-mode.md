# Encrypt 重建 · 工作模式清单

> **分支**：`chore/thirdparty-add-cannbot-skills`（及后续 Encrypt 重建专用支）。  
> **参考线**（方法，不迁旧 Encrypt 核）：  
> - `origin/cursor/cann-ntt-operator-refactor-fe53` — 图谱 + cannbot + `graph-tests`  
> - `origin/cursor/kem-2launch-sticky-1534` — 实机 TRACE/粘性取证与失败沉淀  
> **目标**：用 cannbot-skills + 已有 NTT/SHA3 等积木 **重拼 Alg.14 Encrypt**；**最终须 NPU 真机通过**（远程直连为后话，当前可走 CANNLab/用户上机）。

---

## 1. 角色

| 角色 | 做 | 不做 |
|------|----|------|
| **主控（本会话）** | 读 cannbot skills；维护 KB + DAG；设计实验刀；写任务书；派 **Task subagent**；回收反馈；刷新 KB/DAG；解读 NPU 结果 | **不写** kernel / `graph-tests` 用例实现代码；不否决 cannbot 脚本红线 |
| **编码 Subagent** | 按任务书在**新目录**编码；跑 CPU/SIM（及获权后的 NPU 步骤）；交回 STATUS + 日志摘要 + sync_audit JSON | 不定目标；**不改** KB/DAG yaml；不抄禁抄树；超时须回报 |
| **用户** | 开远程 NPU / CANNLab；授权上机；反馈打字结果；拍板范围 | 不填复杂表 |

---

## 2. 目录约定

| 路径 | 职责 |
|------|------|
| `docs/notes/Encrypt-cannbot-rebuild-capability-inventory.md` | 积木与缺口 |
| `docs/notes/Encrypt-cannbot-rebuild-work-mode.md` | 本文件 |
| `docs/notes/Encrypt-cannbot-rebuild-kb.md` | **唯一**短知识库（失败优先） |
| `docs/rg-encrypt-cannbot-rebuild.yaml` | **唯一**机读 DAG |
| `graph-tests/` | 试验场实现：`toys/` · `bricks/` · `enc_related/` |
| `graph-tests/encrypt-rebuild-ops/` | **任务书 / FEEDBACK / logs**；[`RHYTHM.md`](../../graph-tests/encrypt-rebuild-ops/RHYTHM.md) 双轨节奏 |
| `.cannbot/mlkem-pke-encrypt-rebuild/` | cannbot 过程件（需求/审计 JSON/state） |
| `thirdparty/cannbot-skills/` | 只读引用；**不** vendor 进 `.cursor/skills`（除非用户当次确认） |

旧 `examples/**/encrypt|encaps|decaps`、`pass-fix-f203-alg14|20|21*`：**冻结只读**，可 `sync_audit` 对照，**禁止 fork 源码**。

---

## 3. cannbot 技能路由（设计用）

| 阶段 | 主控必载 | 产出 |
|------|----------|------|
| 环境 / 架构 | `npu-arch`、`ascendc-env-check`（文档） | 目标 Soc=910B3/B4 → **DAV_2201**；单卡 `ASCEND_DEVICE_ID=0`（CANNLab） |
| 需求 / 设计 | `ascendc-docs-gen`、`ascendc-tiling-design`、`ascendc-api-best-practices`（含 CrossCore） | `.cannbot/.../01-requirement.md` 等（按需） |
| 编码门禁 | **`ascendc-sync-audit`**（每刀必跑） | JSON → `.cannbot/.../tmp/`；红线原样进 STATUS |
| 卡死分诊 | `ascendc-crash-debug`、sync-audit `deadlock-triage` | 挂点假设 → 新 DAG 节点 |
| 工程壳 | `ascendc-direct-invoke-template`（**Vector/add_custom 壳**；910B 用本仓 run.sh 惯例，勿盲目 Blaze A5 模板） | 新目录脚手架 |
| 精度（后置） | `ascendc-precision-debug` | **卡死未清 / 骨架未通前不启** |

**禁止**：盲目跑会改写根 `AGENTS.md` 的完整 `init.sh`。

---

## 4. 实验环（强制）

```text
主控：查 KB + DAG → cannbot 设计本刀假设与验收
  → 写/更新 graph-tests/encrypt-rebuild-ops/tasks/<ID>/TASK.md
  → Task subagent 在 TASK 指定代码目录编码 + SIM（获权则 NPU）
  → 回收：FEEDBACK.md + logs/ + 实现 STATUS
  → 主控刷新 KB（成功台账 + 失败 X*）与 DAG；更新 QUEUE.md 状态
  → python3 scripts/check_rg_dag.py --yaml docs/rg-encrypt-cannbot-rebuild.yaml
```

任务书最小字段见 `encrypt-rebuild-ops/_templates/TASK.md`；全局禁令见 `COMMON.md`。

| 规则 | 说明 |
|------|------|
| 一刀一目录 | 坏了回退，不在失败目录上叠屎山；任务书在 `encrypt-rebuild-ops/`，实现另目录 |
| SIM 为主过程证据 | CPU 不作卡死因果依据（可作编译冒烟） |
| 失败一等公民 | 失败刀必须沉 KB + DAG；禁止只堆绿 |
| 禁否决 sync_audit 红线 | SIM 绿 ≠ 同步干净 |
| 上机门禁 | SIM 假设证够 + 失败对照齐 + 半页复现步骤后，再请 NPU |
| Subagent 可读 | TASK 中写明必读 KB 节与 DAG 节点 ID；**禁止**其改 yaml/kb |
| 无 NPU 可推进 | Wave A–C 只跑 CPU+SIM；Wave D 预写为 `wait_npu` |

---

## 5. 下发任务书

- 预写队列：[`graph-tests/encrypt-rebuild-ops/QUEUE.md`](../../graph-tests/encrypt-rebuild-ops/QUEUE.md)  
- 单刀：`tasks/<ID>/TASK.md` + 回收 `FEEDBACK.md` + `logs/`  
- 模板：`_templates/TASK.md` · `_templates/FEEDBACK.md`

---

## 6. 与「旧全链交付」的切割

| 旧路径 | 本模式 |
|--------|--------|
| 在 stable/exp 上改 Encrypt / 对齐 liboqs 为第一刀 | **graph-tests 积木拼装**；正确性可后置阶段，但 **NPU 不挂 + I/O 最终要对** |
| Agent 自己写满核再跑 | **主控不编码**；subagent 编码 |
| 凭记忆抄邻近 encrypt | **cannbot + 独立积木 + KB 契约** |

---

**状态**：工作模式已锁定；任务队列已预写时可待派发；KB/DAG 随刀刷新。
