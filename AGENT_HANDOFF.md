# AGENT_HANDOFF

**日期**：2026-09-11  
**分支**：`cursor/kem-2launch-sticky-1534`（已合入 `chore/thirdparty-add-cannbot-skills` + `cursor/cann-ntt-operator-refactor-fe53` + `cursor/launch-reduce-npu-pass-23e1`）

---

## ★ 定位

停既有 l18 debug → 新写 PKE/KEM；本侧验收/测试。  
**合 main**：收官 docs/qa（带 `__br-kem-2launch-sticky-1534`）；**scripts 不合 main**。

收官包：见 `docs/research/` 下带 `__br-kem-2launch-sticky-1534` 的 Encrypt NPU 粘性挂收官目录。

### 本分支已收敛的 Agent 线

1. **kem-2launch-sticky-1534**：Encrypt NPU 粘性挂探索与收官文档。  
2. **thirdparty-add-cannbot-skills**：cannbot-skills / reasoning-graph-skill、KeyGen·Decrypt·Encrypt cannbot 重建 KB/图谱。  
3. **cann-ntt-operator-refactor-fe53**：Encrypt×cann-ntt Host 编排 EN01–EN12（NPU 不挂）及相关文档/探针。  
4. **launch-reduce-npu-pass-23e1**：Launch 压缩六档上板 + **PKE Decrypt 单 Host launch 融合核** profiling 收口（见下）。

> 未提交的 HiDevLab 上机脚本仍在 stash：`wip before merge-into kem-2launch (fe53 hidevlab)`。

---

## ★ 已合入：Launch 压缩 / Decrypt 单 launch profiling

入口：[`graph-tests/launch-reduce-ops/PLAN.md`](graph-tests/launch-reduce-ops/PLAN.md)  
总表：[`qa/active_npu_perf_summary.md`](qa/active_npu_perf_summary.md)（表头用实验全名，不用暗号）

**PKE Decrypt · 单 Host launch 融合核** profiling **已收口**（[`docs/perf/round_003_pke_decrypt_1launch/`](docs/perf/round_003_pke_decrypt_1launch/)）：

- cycle：vector0 **scalar ≈ 96.8%** → scalar/握手等待主导  
- 应用级 Chrome Trace JSON：**有**（`msprof_*.json`）  
- HTML 报告：**有**（details/roofline/cache/raw-data）  
- 核内指令 Timeline：**无**（TimelineDetail dump 失败 + CLI 无 PipeTimeline；工具链限制，非漏跑）

### 下一刀（等指令）

1. 基于 cycle 证据，改 **PKE Decrypt 融合核** 的 scalar/跨核握手（算法/实现刀，不是继续采图）；或  
2. 断板收工。
