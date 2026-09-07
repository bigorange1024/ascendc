> **最后刷新**：2026-09-07（ER01 PASS；下一刀 ER02 清 cannbot SYNC-02 红线）

---

## ★ 给新 Agent 的 60 秒上手

1. **主线**：图谱 + 主动用 `thirdparty/cannbot-skills`；核心 = NPU 不卡死。  
2. **KB/DAG**：[`Encrypt-hang-rewrite-kb.md`](docs/notes/Encrypt-hang-rewrite-kb.md) · [`rg-encrypt-hang-rewrite.yaml`](docs/rg-encrypt-hang-rewrite.yaml)  
3. **已完成**：toys T01–T07；**ER01 PASS**（`graph-tests/enc_related/ER01-encrypt-shaped-2launch-skel/`）  
4. **P0**：ER02 — 按 cannbot 清 ER01 的 **SYNC-02 红线**（禁否决脚本候选），再考虑加体量  
5. **910B3**：未经明示不连。

### 待办快照

| 项 | 说明 |
|----|------|
| **P0** | **ER02**：修 SYNC-02；复跑 sync_audit + CPU/SIM |
| **ER01** | PASS；学到：Encrypt 外形 2launch+GATE+真MAC 在 SIM 仍不挂（X24） |
| **非目标** | 正确性；抄旧 Encrypt；vendor skills |

**别做**：否决 cannbot 红线；同质空转加压；5/7；Wait 中 SyncAll。
