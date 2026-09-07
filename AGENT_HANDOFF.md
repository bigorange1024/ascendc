> **最后刷新**：2026-09-07（用户白话锁定：图谱+cannbot；开 ER01；核心仍卡死）

---

## ★ 给新 Agent 的 60 秒上手

1. **主线**：图谱实验做 Encrypt 不卡死；**主动用** `thirdparty/cannbot-skills`（不必装进 `.cursor/skills`）。  
2. **真相**：KB [`Encrypt-hang-rewrite-kb.md`](docs/notes/Encrypt-hang-rewrite-kb.md) · DAG [`rg-encrypt-hang-rewrite.yaml`](docs/rg-encrypt-hang-rewrite.yaml) · 用户结论 [`.cannbot/mlkem-pke-encrypt/CP1-用户结论.md`](.cannbot/mlkem-pke-encrypt/CP1-用户结论.md)  
3. **下一刀**：[ER01-TASK.md](graph-tests/enc_related/ER01-TASK.md)  
4. **910B3**：未经明示不连。  
5. **Git**：无明确指令不 commit/push。

### 待办快照

| 项 | 说明 |
|----|------|
| **P0** | **实现 ER01**（enc_related 双段 launch 骨架 + cannbot sync_audit）；SIM 不挂 |
| **已锁定** | 图谱继续；cannbot 主动用；核心=卡死；不 vendor skills |
| **非目标** | 正确性/liboqs；抄旧 Encrypt；ACLNN 大工程 |

**别做**：同质 toys；5/7；Wait 中 SyncAll；自造 SoftSync；要用户传文件。
