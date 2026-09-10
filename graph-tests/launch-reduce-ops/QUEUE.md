# Launch 压缩 · QUEUE

| # | ID | 状态 | 说明 |
|---|-----|------|------|
| 1 | LR-KG-F1 | **PASS_NPU×30** | PKE KeyGen 3→2（RB-K07）；Σ 1127µs 见 [PERF](PERF.md) |
| 2 | LR-KG-F2b | **PASS_NPU×30** | KEM KeyGen 4→3（RB-K08） |
| 3 | LR-DC-F1 | **PASS_NPU×30** | Decrypt 3→2（RB-D08） |
| 4 | LR-KG-F2 | **PASS_NPU×30** | KEM →2（RB-K09）；Σ 1227µs |
| 5 | LR-DP-F1 | **PASS_NPU×30** | Decaps →4（T28 融合 Decrypt + Encaps2） |
| 6 | LR-DP-F2 | **PASS_NPU×30** | Decaps →3（T29 prep 融 MIX） |
| 7 | LR-DC-F2 | **PASS_NPU×30** | Decrypt →1（RB-D09）；Σ **456µs** |
| 8 | LR-DP-F3 | **PASS_NPU×30** | Decaps →2（RB-T30）；Σ 1719µs |

性能填表权威：[`PERF.md`](PERF.md)（对齐教材三层口径）。