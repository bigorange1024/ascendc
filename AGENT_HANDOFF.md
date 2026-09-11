# AGENT_HANDOFF

**日期**：2026-09-11  
**分支**：`cursor/kem-2launch-sticky-1534`（已合入 cannbot-skills / cann-ntt-refactor / launch-reduce 等）

---

## ★ 定位

停既有 l18 debug → 新写 PKE/KEM；本侧验收/测试。  
**合 main**：收官 docs/qa（带 `__br-kem-2launch-sticky-1534`）；**scripts 不合 main**。

### 本分支已收敛的 Agent 线

1. **kem-2launch-sticky-1534**：Encrypt NPU 粘性挂探索与收官文档。  
2. **thirdparty-add-cannbot-skills**：cannbot-skills / reasoning-graph-skill。  
3. **cann-ntt-operator-refactor-fe53**：Encrypt×cann-ntt Host 编排。  
4. **launch-reduce-npu-pass-23e1**：Launch 压缩六档 + Decrypt 1-launch profiling 收口。

---

## ★ 当前 P0（可上机）

**战役**：PKE Decrypt 融合核 · **scalar / 跨核握手优化**  
**入口**：[`graph-tests/decrypt-scalar-opt/INDEX.md`](graph-tests/decrypt-scalar-opt/INDEX.md)  
**计划**：[`PLAN.md`](graph-tests/decrypt-scalar-opt/PLAN.md) · **队列**：[`QUEUE.md`](graph-tests/decrypt-scalar-opt/QUEUE.md)  
**推理图谱**：[`docs/rg-decrypt-scalar-opt.yaml`](docs/rg-decrypt-scalar-opt.yaml)

| 步 | 状态 |
|----|------|
| 计划 + 图谱 + W1 静态热点 | **已完成** |
| 推荐上板序 | **H2 → H3 → H1 → H4** |
| DS-W0 板上基线复测 | **待上机**（下一动作） |
| DS-H2 新树 `RB-D10b-prep-vec` | 阻塞于 W0 |

**基线证据**：vector0 scalar ≈ 96.8%；Σ ≈ 455 µs；产物 `docs/perf/round_003_pke_decrypt_1launch/`。  
**总表**：[`qa/active_npu_perf_summary.md`](qa/active_npu_perf_summary.md)。

### 硬约束

- 只在本 sticky 分支改；**禁止擅自开分支**。  
- 新树 `RB-D10*`；**禁改** D08/D09/T28–T30 源码。  
- 结案仅 NPU；flag∈{1,3}；禁 SoftSync / Wait 环 SyncAll。

---

## ★ 非 P0

- 再采核内指令 Timeline（工具链已证伪）。  
- 再压 Decrypt Host launch（已是 1）。  
- 断板：等用户指令。
