# Launch 压缩 · QUEUE

| # | ID | 状态 | 说明 |
|---|-----|------|------|
| 1 | LR-KG-F1 | **PASS_NPU×30** | PKE KeyGen 3→2（RB-K07） |
| 2 | LR-KG-F2b | **PASS_NPU×30** | KEM KeyGen 4→3（RB-K08） |
| 3 | LR-DC-F1 | **PASS_NPU×30** | Decrypt 3→2（RB-D08） |
| 4 | LR-KG-F2 | **PASS_NPU×30** | KEM →2（RB-K09，kem_tail 进 MIX） |
| 5 | LR-DP-F1 | **PASS_NPU×30** | Decaps →4（T28 融合 Decrypt + Encaps2） |
| 6 | LR-DP-F2 | **PASS_NPU×30** | Decaps →3（T29 prep 融 MIX） |
| 7 | LR-DC-F2 | **PASS_NPU×30** | Decrypt →1（RB-D09 新建树；不改 D08） |
| 8 | LR-DP-F3 | **IN_PROGRESS** | Decaps →2（RB-T30 新建树；不改 T29） |
