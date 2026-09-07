# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：新 Cloud / 本地 Agent 的**唯一短真相**；本文件优先于长对话历史。  
> **入口**：[`AGENTS.md`](AGENTS.md) → **本文件** → Rule / Skill。  
> **最后刷新**：2026-09-07（Encrypt：**cannbot 直调从头做**；910B3 已通但禁擅自连；等 CP1 拍板）

---

## ★ 给新 Agent 的 60 秒上手

1. **当前主线 = Encrypt 用 cannbot 直调从头做**  
   - 绑定：[`docs/plans/2026-09-07-Encrypt-cannbot直调开发绑定.md`](docs/plans/2026-09-07-Encrypt-cannbot直调开发绑定.md)  
   - CP1 草稿：[`.cannbot/mlkem-pke-encrypt/01-requirement.md`](.cannbot/mlkem-pke-encrypt/01-requirement.md)  
   - KB / DAG：[`Encrypt-hang-rewrite-kb.md`](docs/notes/Encrypt-hang-rewrite-kb.md) · [`rg-encrypt-hang-rewrite.yaml`](docs/rg-encrypt-hang-rewrite.yaml)  
   - Skills 根：`thirdparty/cannbot-skills/`（只读引用；勿盲目 init.sh）  
2. **角色**：主控按 cannbot CP 编排；编码/SIM 可派 subagent；**不抄旧 Encrypt**。  
3. **纪律**：每刀 sync-audit；失败优先；SIM 穷尽；**910B3 未经明示不连**。  
4. **Git**：无用户明确指令禁 commit/push/开新分支（覆盖 Cloud 默认开 PR 流程）。  
5. 旧 Decrypt / `rg-encrypt-l18`：**只读参考**，勿与本线混做。

### 待办快照

| 项 | 说明 |
|----|------|
| **P0（当次）** | **CP1 拍板**：架构/落点/正确性节奏/Hostμ/是否 vendor cannbot skills（见 `.cannbot/mlkem-pke-encrypt/01-requirement.md` §9） |
| **已完成** | T01–T07 SIM；cannbot 绑定计划 + 需求草稿（X22） |
| **上机** | 910B3 已调通；**未经明示不连**；短窗只跑用户指定命令 |
| **Encrypt 最终** | NPU 不卡死且正确；方法 = cannbot 直调全流程 |
| **非目标（首期）** | ACLNN/图模式/性能打满；抄旧 Encrypt；擅自改根 AGENTS 的 cannbot init |

**别做**：复踩 5/7、Wait 中 SyncAll、自造 SoftSync、抄旧 Encrypt；同质 toys 再派；**要求用户回传文件/tar/日志**；无打字反馈就开 enc_related。

---

## ★ 当前真相（卡死重写）

| 项 | 状态 |
|----|------|
| T01 | **PASS**（SIM 可缺 401 / X13） |
| T02 | **PASS** — 生产 GATE 时序 + 轻体量 |
| T03 | **PASS** — 全 FSM（INTT 复用 1/3） |
| T04 | **PASS** — 体量×10 仍绿（X14） |
| T05 | **PASS** — 2×launch |
| T06 | **PASS** — GATE 真 Vec MAC |
| T07 | **PASS** — SAMPLE→FSM |
| 闸门 | **X15** SIM toys 穷尽；**NPU 一次测套件已备** |
| 失败禁令 | X1–X15 见 KB §3 |
| 积木 | NTT/SHA3/内积可引用拼装；卡死归因编排/时序/体量 |
| 1024/768/512 stable 线 | 另线；本交接不展开 |

纪要：[`qa/2026-09/2026-09-06-Encrypt卡死重写T01与T02.md`](qa/2026-09/2026-09-06-Encrypt卡死重写T01与T02.md)

---

## ★ 下一刀

1. 用户确认 CP1（`.cannbot/mlkem-pke-encrypt/01-requirement.md` §9 五问）。  
2. 通过后写 CP2.2 开发方案（强制 cannbot tiling + `api-crosscore-sync` + 本仓 KB）。  
3. 编码落新目录；每刀 `sync_audit`；**不连 910B3** 除非当次授权短命令。
