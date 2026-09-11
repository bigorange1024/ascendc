# Decrypt scalar / 握手优化 · 战役入口

| 项 | 内容 |
|----|------|
| **状态** | **计划已锁 · 可上机**（2026-09-11） |
| **基线实验** | PKE Decrypt 单 Host launch 融合核 · [`../dec_related/RB-D09-decrypt-1launch/`](../dec_related/RB-D09-decrypt-1launch/) |
| **瓶颈证据** | vector0 **scalar ≈ 96.8%**（msopprof PipeUtilization）；见 [`../../docs/perf/round_003_pke_decrypt_1launch/`](../../docs/perf/round_003_pke_decrypt_1launch/) |
| **计划** | [`PLAN.md`](PLAN.md) |
| **队列** | [`QUEUE.md`](QUEUE.md) |
| **推理图谱** | [`../../docs/rg-decrypt-scalar-opt.yaml`](../../docs/rg-decrypt-scalar-opt.yaml) |
| **性能总表** | [`../../qa/active_npu_perf_summary.md`](../../qa/active_npu_perf_summary.md) |
| **分支** | 只在 `cursor/kem-2launch-sticky-1534` 上改；**禁止擅自开分支** |

## 一句话

Launch 数已经是 1；下一战役不是再压 launch，而是按 cycle 证据把 **scalar / 跨核握手气泡**打掉，降低 Σ Task Duration。

## 硬约束（继承工程 KB）

- 新树新目录（`RB-D10*`）；**禁止改** D08 / D09 / T28 / T29 / T30 源码。  
- `flag ∈ {1,3}`；禁 SoftSync；禁 Wait 环内 `SyncAll`；业务写出禁 `GlobalTensor::SetValue`。  
- 结案真源 **仅 NPU**（正确性 ×30 + ops-profiling / msopprof）；CPU/SIM 不得结案。  
- 汇报用**实验全名**，不用暗号当主称谓。
