# STATUS — RB-T30-decaps-2launch

| 项 | 值 |
|----|-----|
| 刀 | LR-DP-F3（Decaps →2） |
| Host launch | **2** |
| Kernels | `t30_dec_fused_custom` · `t30_enc_fused_custom` |
| Magic | `0x54333032` |
| 状态 | **PASS_NPU×30**（2026-09-10） |

## 验收

| 档 | 结果 |
|----|------|
| NPU 冒烟 | PASS（K≡liboqs，c'≡c，launches=2） |
| NPU×30 | ok=30 fail=0 |
| 日志 | `t30-smoke-20260910-153112.log` · `t30-x30-20260910-153535.log` |

## 性能（NPU · 设备真值）

口径见 [`../../launch-reduce-ops/PERF.md`](../../launch-reduce-ops/PERF.md)。

| 项 | 值 |
|----|-----|
| Σ Task Duration | **1719.22 µs**（1.719 ms） |
| `t30_dec_fused_custom` | 455.12 µs |
| `t30_enc_fused_custom` | 1264.10 µs |
| 采集 | ops-profiling 2026-09-10 · `docs/perf/round_001/` |
| 较长 MIX | cube_util 3.85% · AIV scalar 54.8% · 头开销 11.5% |