# Agent 交接（Launch 压缩 · NPU）

> **最后刷新**：2026-09-10（ops-profiling 六档设备 Task Duration 收口；keepalive 已停）  
> **分支**：`cursor/launch-reduce-npu-pass-23e1`  
> **入口**：[`graph-tests/launch-reduce-ops/PLAN.md`](graph-tests/launch-reduce-ops/PLAN.md)

---

## ★ 当前真相

1. Launch 压缩 **QUEUE 1–8 全绿**：KG=2、Decrypt=1、Decaps=**2**（**优于** stable Decaps=3）。  
2. **性能口径纠正**：只认 `thirdparty/cannbot-skills/ops/ops-profiling`（`msprof_profile_run.sh` + `msprof_perf_summary.py`）；**禁止** host `wall_sec`。  
3. **910B3 设备 Σ Task Duration**（PipeUtilization）：K07 1127µs · K09 1227µs · T19 1239µs · T23 1284µs · D09 **456µs** · T30 1719µs。  
4. 远端：`/mnt/workspace/launch-reduce-logs/ops-profiling-rebuild-20260910-160409/`（`index.csv` 全 `summary_rc=0`）；本地摘要见 artifacts `ops-profiling-rebuild/`。  
5. keepalive **已停**（放机）。

---

## ★ 开机后立刻做

1. `which_npu.sh`（勿残留旧 `SSH_HOST_FORCE`）→ 需上板再 keepalive  
2. 可选：`ascend-profiling-anomaly` 深挖（AIV scalar 高 / cube_util 低）  
3. 可选 Wave4 经验入库（改 KB 须授权）  
4. 无新刀则保持放机
