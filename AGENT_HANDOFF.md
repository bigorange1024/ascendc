# Agent 交接（Launch 压缩 · NPU）

> **最后刷新**：2026-09-10（性能文档按教材三层口径落盘；keepalive 已停）  
> **分支**：`cursor/launch-reduce-npu-pass-23e1`  
> **入口**：[`graph-tests/launch-reduce-ops/PLAN.md`](graph-tests/launch-reduce-ops/PLAN.md)

---

## ★ 当前真相

1. Launch 压缩 **QUEUE 1–8 全绿**：KG=2、Decrypt=1、Decaps=**2**（优于 stable Decaps=3）。  
2. **性能记录**（工程口径）：见 [`docs/research/教材KEM实机测量清单.md`](docs/research/教材KEM实机测量清单.md) 三层表 + 战役 [`graph-tests/launch-reduce-ops/PERF.md`](graph-tests/launch-reduce-ops/PERF.md) / [`COMMON.md`](graph-tests/launch-reduce-ops/COMMON.md)。  
3. **设备真值**（910B3 · Task Duration Σ）：K07 1127µs · K09 1227µs · T19 1239µs · T23 1284µs · D09 **456µs** · T30 1719µs。各用例 `STATUS.md` 已写「性能」节。  
4. keepalive **已停**（放机）。

---

## ★ 开机后立刻做

1. `which_npu.sh`（勿残留旧 `SSH_HOST_FORCE`）→ 需上板再 keepalive  
2. 可选：挂 `[npu_launch]` 补 Host 层；或 `ascend-profiling-anomaly` 深挖  
3. 可选 Wave4 经验入库（改 KB 须授权）  
4. 无新刀则保持放机
