# STATUS — RB-K08-kem-3launch

| 字段 | 值 |
|------|-----|
| 刀 | LR-KG-F2b · KEM KeyGen 三 launch |
| 状态 | **PASS_NPU×30**（2026-09-10） |
| 真机 | Ascend910B3 · ASCEND_DEVICE_ID=0 |
| 证据 | PASS_SYNC+PASS_IO；ek/dk_kem max_abs=0；×30 ok=30 |

Host 3 launch：prep → 融合 MIX（NTT+dot/encode）→ kem_tail。
