# TASK-E18 — toy-e18-l18-fsm-sep56

**图谱**：`D-exp-e18` · 规格：`graph_tests/ENCRYPT_GAP.md`  
**deadline_min**: 35  
**max_retries**: 1  
**silent_hang_min**: 10  
**abort_on**: [timeout, no_progress, scope_breach, sim_hang]

---

## 目标（相对 E17 单因子）

新建 `graph_tests/toys/toy-e18-l18-fsm-sep56/`：与 **E17 同序**，**仅**把伪 INTT 的 CrossCore 从复用 **1/3** 改为独立 **5/6**：

```
伪 NTT：仍 1/3
GATE：  仍 4/8
伪 INTT：AIV SET(5) ↔ AIC Wait(5)；AIC SET(6) ↔ AIV Wait(6)   ← 唯一差分
```

推荐做法：从 `toy-e17-l18-fsm-reuse13/` **复制工程壳**后只改 INTT 段 flag 与 TRACE 号/注释/magic（如 `E18TOY01`/`0xE8`），避免漂移。

---

## 白名单

- 仅可写：`graph_tests/toys/toy-e18-l18-fsm-sep56/**`  
- FEEDBACK：`graph_tests/_outbox/FEEDBACK-E18.md`  
- 可读：E17 目录、TASK-E17、ENCRYPT_GAP、SUBAGENT_RULES、cannbot  

禁止：改 E17；改图谱/知识库；commit/push；上机；并行 SIM；抄 Encrypt。

---

## 验收

1. `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4`（默认 ≥8 轮，Host 末 `111`）  
2. sync_audit → `/opt/cursor/artifacts/e18-sync-audit.json`  
3. `STATUS.md` `TRACE.md`（写明相对 E17 单因子）  

本线 **SIM only**。

## FEEDBACK

- PASS/FAIL、wall、轮次  
- 与 E17 差分确认（INTT 5/6）  
- `D-exp-e18` support/weaken  
