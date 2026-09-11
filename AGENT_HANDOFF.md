# AGENT_HANDOFF

**日期**：2026-09-11  
**分支**：`cursor/kem-2launch-sticky-1534`（已合入 `chore/thirdparty-add-cannbot-skills` + `cursor/cann-ntt-operator-refactor-fe53`）

---

## ★ 定位

停既有 l18 debug → 新写 PKE/KEM；本侧验收/测试。  
**合 main**：收官 docs/qa（带 `__br-kem-2launch-sticky-1534`）；**scripts 不合 main**。

收官包：见 `docs/research/` 下带 `__br-kem-2launch-sticky-1534` 的 Encrypt NPU 粘性挂收官目录。

### 本分支已收敛的 Agent 线

1. **kem-2launch-sticky-1534**：Encrypt NPU 粘性挂探索与收官文档。  
2. **thirdparty-add-cannbot-skills**：cannbot-skills / reasoning-graph-skill、KeyGen·Decrypt·Encrypt cannbot 重建 KB/图谱。  
3. **cann-ntt-operator-refactor-fe53**：Encrypt×cann-ntt Host 编排 EN01–EN12（NPU 不挂）及相关文档/探针。

> 未提交的 HiDevLab 上机脚本仍在 stash：`wip before merge-into kem-2launch (fe53 hidevlab)`。
