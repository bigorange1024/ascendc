# STATUS — RB-D08-decrypt-2launch

| 字段 | 值 |
|------|-----|
| 刀 | LR-DC-F1 · Decrypt 二 launch |
| 状态 | **PASS_NPU×30**（2026-09-10） |
| 真机 | Ascend910B3 · ASCEND_DEVICE_ID=0 |
| 证据 | PASS_SYNC+PASS_IO；m max_abs=0；×30 ok=30 |

Host 2 launch：prep → 融合 MIX（NTT+INTT）。关键：融合 ws 段偏移 32B 对齐。
