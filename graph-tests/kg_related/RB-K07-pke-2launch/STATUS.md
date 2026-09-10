# STATUS — RB-K07-pke-2launch

| 字段 | 值 |
|------|-----|
| 刀 | LR-KG-F1 · PKE KeyGen 二 launch 全链 |
| 状态 | **PASS_NPU×30**（2026-09-10） |
| 真机 | Ascend910B3 · ASCEND_DEVICE_ID=0 |
| 证据 | PASS_SYNC + PASS_IO；ek/dk max_abs=0 vs liboqs_pke_ref；×30 ok=30 |

Host：`kg_prep_custom` → mid-sync → `kg_ntt_dot_encode_custom` → sync（2 launch）。
