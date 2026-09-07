# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：新 Cloud / 本地 Agent 的**唯一短真相**；本文件优先于长对话历史。  
> **入口**：[`AGENTS.md`](AGENTS.md) → **本文件** → Rule / Skill。  
> **最后刷新**：2026-09-07（**交接新 Cloud Agent**：推干净分支；P0=ER02；secrets 已更新）  
> **分支**：`cursor/cann-ntt-operator-refactor-fe53`（基线 `main`）

---

## ★ 给新 Agent 的 60 秒上手

1. **主线**：Encrypt **不卡死**重写 — 走 **`graph-tests/` 图谱实验** + **主动用** `thirdparty/cannbot-skills`（不必装进 `.cursor/skills`）。  
2. **必读**：  
   - KB [`docs/notes/Encrypt-hang-rewrite-kb.md`](docs/notes/Encrypt-hang-rewrite-kb.md)（尤其 X15/X21–X24）  
   - DAG [`docs/rg-encrypt-hang-rewrite.yaml`](docs/rg-encrypt-hang-rewrite.yaml)（活跃：`D-CANNBOT-ENC-RELATED`、`D-EXP-ER02`）  
   - 用户锁定 [`.cannbot/mlkem-pke-encrypt/CP1-用户结论.md`](.cannbot/mlkem-pke-encrypt/CP1-用户结论.md)  
   - cannbot 绑定 [`docs/plans/2026-09-07-Encrypt-cannbot直调开发绑定.md`](docs/plans/2026-09-07-Encrypt-cannbot直调开发绑定.md)  
3. **开干前**：`bash scripts/clone-thirdparty.sh`（清单已含 **cannbot-skills**；新 secrets 后若缺再拉）。  
4. **910B3**：用户已有云主机但机时紧；**未经当次明示禁止连 NPU**。  
5. **Git**：用户本轮已要求推送本分支；后续仍遵守「无指令不乱开分支」；改完可按用户授权提交。

### 待办快照（P0）

| 项 | 说明 |
|----|------|
| **P0** | **ER02**：修 ER01 的 cannbot **SYNC-02 红线**（16 条，禁否决脚本候选）；复跑 `sync_audit` + `bash run.sh -r cpu` + `SIM_DIRECT=1 bash run.sh -r sim` |
| **ER01** | **PASS** — `graph-tests/enc_related/ER01-encrypt-shaped-2launch-skel/`（CPU+SIM 不挂；STATUS 含红线原文） |
| **审计 JSON** | `.cannbot/mlkem-pke-encrypt/tmp/ER01-sync_audit.json` · 旧核对照 `tmp/sync_audit_l18_l19.json` |
| **核心目标** | NPU Encrypt **SynchronizeStream 不再卡死**；**正确性非本阶段门禁** |
| **非目标** | liboqs 对齐；抄旧 Encrypt；vendor skills 进 `.cursor/skills`；盲目 `cannbot init.sh` 改写根 AGENTS |

**别做**：否决 cannbot 红线；同质 toys 空转；flag 5/7；AIC Wait 中 SyncAll；自造 SoftSync；要用户回传文件。

---

## ★ 当前真相

| 项 | 状态 |
|----|------|
| toys T01–T07 | **PASS**（X15：同质 SIM toys 穷尽） |
| ER01 | **PASS**（Encrypt 形 2launch + 生产 GATE + 真 Vec MAC；SIM 可缺 401=X13） |
| ER02 | **已下令未做**（清 SYNC-02） |
| 方法 | 图谱 + cannbot sync-audit / CrossCore 文档 / crash-debug（按需） |
| 旧 Encrypt | 冻结只读；可审计对照，禁抄核 |

纪要：[`qa/2026-09/2026-09-06-Encrypt卡死重写T01与T02.md`](qa/2026-09/2026-09-06-Encrypt卡死重写T01与T02.md)（含 09-07 实机秒失败笔记）

---

## ★ 下一刀（给接手 Agent）

1. 读 ER01 `STATUS.md` 红线列表与 `aiv_func.hpp` / `mmad_custom.cpp` 对应行。  
2. 按 cannbot `ascendc-sync-audit` 的 fix-patterns（SetFlag/WaitFlag 或 EnQue/DeQue）修 SYNC-02；**不要**用「SIM 已绿」否决红线（X24）。  
3. 复跑 sync_audit：红线应清零或仅剩已标注能力边界。  
4. CPU + SIM 双过后再定 ER03（近生产体量 / 真积木加码）；上机须用户点名。  
5. 每刀结束刷新本文件 + KB/DAG。

### 常用命令

```bash
bash scripts/clone-thirdparty.sh
cd graph-tests/enc_related/ER01-encrypt-shaped-2launch-skel
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
python3 thirdparty/cannbot-skills/ops/ascendc-sync-audit/scripts/sync_audit.py \
  mmad_custom.cpp aiv_func.hpp aic_func.hpp basic.hpp kyber_limb6.hpp tiling.h --format json
```
