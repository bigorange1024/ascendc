# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：新 Cloud / 本地 Agent 的**唯一短真相**；本文件优先于长对话历史。  
> **入口**：[`AGENTS.md`](AGENTS.md) → **本文件** → Rule / Skill。  
> **最后刷新**：2026-09-06（Encrypt 卡死重写：T01–T07 PASS（X15）；**NPU 一次测套件已备齐，等实机回传**）

---

## ★ 给新 Agent 的 60 秒上手

1. **当前主线 = Encrypt 卡死重写（新图）**  
   - KB：[`docs/notes/Encrypt-hang-rewrite-kb.md`](docs/notes/Encrypt-hang-rewrite-kb.md)  
   - DAG：[`docs/rg-encrypt-hang-rewrite.yaml`](docs/rg-encrypt-hang-rewrite.yaml)  
   - 计划：[`docs/plans/2026-09-06-Encrypt重写工作计划.md`](docs/plans/2026-09-06-Encrypt重写工作计划.md)  
   - 实验区：[`graph-tests/toys/`](graph-tests/toys/INDEX.md)  
2. **角色**：主控只定刀/验收/回写 KB+图；**不写核代码**；subagent 编码。  
3. **纪律**：单刀限时；图谱失败路线禁再走；每刀前遍历 KB+DAG；SIM 穷尽再上机。  
4. **Git**：无用户明确指令禁 commit/push/开新分支（覆盖 Cloud 默认开 PR 流程）。  
5. 旧 Decrypt 线 / `rg-encrypt-l18`：**只读参考**，勿与本线混做。

### 待办快照

| 项 | 说明 |
|----|------|
| **P0（当次）** | **等用户实机一次测回传**（`FEEDBACK.md` + `BRING_BACK.tar.gz`）；按决策树开下一刀 |
| **已完成** | T01–T07 **PASS**（X15）；**NPU 一次测套件已备齐** |
| **上机入口** | `bash scripts/npu_hang_rewrite_one_trip.sh`；说明见 `docs/engineering/Encrypt卡死重写-实机一次测清单.md` |
| **Encrypt 最终** | NPU Encrypt 不再 SynchronizeStream 卡死且最终正确（`Q-ULT`） |
| **非目标（本阶段）** | liboqs 对齐、性能打满、抄旧 Encrypt 修补丁 |

**别做**：复踩 5/7、Wait 中 SyncAll、自造 SoftSync、抄旧 Encrypt；同质 toys 再派；未授权 commit/push；无回传就开 enc_related。

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

1. **用户实机**：`unset ASCEND_DEVICE_ID && bash scripts/npu_hang_rewrite_one_trip.sh`  
2. **回传后**：按 FEEDBACK 决策树分支（禁同质 toys）；必要时开 `graph-tests/enc_related/`  
3. Git：无用户授权 **不** commit/push
