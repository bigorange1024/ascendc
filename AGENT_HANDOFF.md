# Agent 交接（Launch 压缩 · NPU）

> **最后刷新**：2026-09-11（**PKE Decrypt 单 Host launch 融合核** profiling 收口完成）  
> **分支**：`cursor/launch-reduce-npu-pass-23e1`  
> **入口**：[`graph-tests/launch-reduce-ops/PLAN.md`](graph-tests/launch-reduce-ops/PLAN.md)

---

## ★ 当前真相

1. Launch 压缩六档已上板；总表 [`qa/active_npu_perf_summary.md`](qa/active_npu_perf_summary.md)（表头已改为**实验全名**，不用暗号）。  
2. **板在线**：`cannlab-npu`；物理 davinci6 → `ASCEND_DEVICE_ID=0`；Freq 1800/1800。  
3. **PKE Decrypt · 单 Host launch 融合核** profiling **已收口**（详见 [`docs/perf/round_003_pke_decrypt_1launch/`](docs/perf/round_003_pke_decrypt_1launch/)）：
   - cycle：vector0 **scalar ≈ 96.8%** → scalar/握手等待主导  
   - 应用级 Chrome Trace JSON：**有**（`msprof_*.json`）  
   - HTML 报告：**有**（details/roofline/cache/raw-data）  
   - 核内指令 Timeline：**无**（TimelineDetail dump 失败 + CLI 无 PipeTimeline；工具链限制，非漏跑）

---

## ★ 下一刀（等指令）

1. 基于 cycle 证据，改 **PKE Decrypt 融合核** 的 scalar/跨核握手（这是算法/实现刀，不是继续采图）；或  
2. 断板收工。
