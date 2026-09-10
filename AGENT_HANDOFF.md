# Agent 交接（Launch 压缩 · NPU）

> **最后刷新**：2026-09-10（D09 msopprof 真实 cycle 已登；Timeline dump 失败；keepalive 仍开）  
> **分支**：`cursor/launch-reduce-npu-pass-23e1`  
> **入口**：[`graph-tests/launch-reduce-ops/PLAN.md`](graph-tests/launch-reduce-ops/PLAN.md)

---

## ★ 当前真相

1. QUEUE 1–8 全绿；NPU 性能总表 [`qa/active_npu_perf_summary.md`](qa/active_npu_perf_summary.md)。  
2. **D09 深采**：Freq 1800/1800；cube0 **722923** cyc、vector0 **813361** cyc（scalar≈**97%**）→ SCALAR/握手主导。  
3. Timeline：无 `PipeTimeline`；`TimelineDetail` dump 失败 → 尚无 JSON 时间轴图。  
4. 板：`cannlab-npu`（100.107.100.36），物理 NPU7→`ASCEND_DEVICE_ID=0`；keepalive 开着。

---

## ★ 开机后立刻做

1. （可选）修 TimelineDetail dump，或直接针对 vector0 scalar 优化  
2. 无新刀则说一声放机（停 keepalive）
