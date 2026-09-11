# STATUS — RB-K09-kem-2launch

| 字段 | 值 |
|------|-----|
| 刀 | LR-KG-F2 · KEM KeyGen 两 launch |
| 状态 | **PASS_NPU×30**（2026-09-10） |
| 真机 | Ascend910B3 · ASCEND_DEVICE_ID=0 |
| 证据 | PASS_SYNC+PASS_IO；ek/dk_kem max_abs=0；×30 ok=30 fail=0；无 Launch3 |

Host 2 launch：prep → 融合 MIX（NTT+dot/encode+kem_tail）。

## 性能（NPU · 设备真值）

登记见 [`qa/active_npu_perf_summary.md`](../../../qa/active_npu_perf_summary.md)。

| 项 | 值 |
|----|-----|
| Σ Task Duration | **1227.12 µs**（1.227 ms） |
| `kg_prep_custom` | 447.16 µs |
| `kg_ntt_dot_encode_custom` | 779.96 µs |
| 采集 | ops-profiling 2026-09-10 · `docs/perf/round_001/` |
| 主 MIX | cube_util 2.47% · AIV scalar 64.7% · 头开销 25.3% |