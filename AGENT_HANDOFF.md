# Agent 交接（Launch 压缩 · NPU）

> **最后刷新**：2026-09-10（NPU 性能登记表落盘；keepalive 已停）  
> **分支**：`cursor/launch-reduce-npu-pass-23e1`  
> **入口**：[`graph-tests/launch-reduce-ops/PLAN.md`](graph-tests/launch-reduce-ops/PLAN.md)

---

## ★ 当前真相

1. Launch 压缩 **QUEUE 1–8 全绿**：KG=2、Decrypt=1、Decaps=**2**。  
2. **NPU 性能登记**：[`qa/active_npu_perf_summary.md`](qa/active_npu_perf_summary.md)（仿 `active_sim_regress_summary` 版式；**未改** SIM tick 表）。  
3. Σ Task Duration（910B3）：K07 1127 · K09 1227 · T19 1239 · T23 1284 · D09 **456** · T30 1719（µs）。  
4. keepalive **已停**。

---

## ★ 开机后立刻做

1. `which_npu.sh` → 需上板再 keepalive  
2. 新档上板后刷新 `qa/active_npu_perf_summary.md` + 用例 `STATUS`  
3. 无新刀则保持放机
