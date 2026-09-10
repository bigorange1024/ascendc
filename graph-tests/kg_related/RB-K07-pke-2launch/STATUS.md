# STATUS — RB-K07-pke-2launch

| 字段 | 值 |
|------|-----|
| 刀 | LR-KG-F1 · PKE KeyGen 二 launch 全链 |
| 状态 | **PASS_NPU×30**（2026-09-10） |
| 真机 | Ascend910B3 · ASCEND_DEVICE_ID=0 |
| 证据 | PASS_SYNC + PASS_IO；ek/dk max_abs=0 vs liboqs_pke_ref；×30 ok=30 |

Host：`kg_prep_custom` → mid-sync → `kg_ntt_dot_encode_custom` → sync（2 launch）。

## 性能（NPU · 设备真值）

口径见 [`../../launch-reduce-ops/PERF.md`](../../launch-reduce-ops/PERF.md)（对齐教材三层；本行 = Task Duration）。

| 项 | 值 |
|----|-----|
| Σ Task Duration | **1126.58 µs**（1.127 ms） |
| `kg_prep_custom` | 448.16 µs |
| `kg_ntt_dot_encode_custom` | 678.42 µs |
| 采集 | ops-profiling 2026-09-10 · `docs/perf/round_001/` |
| 主 MIX | cube_util 2.83% · AIV scalar 61.5% · 头开销 21.7% |