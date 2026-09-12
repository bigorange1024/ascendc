# EP04 FEEDBACK — Encaps × liboqs 交叉（SIM-only）

| 项 | 值 |
|----|-----|
| 状态 | **PASS** |
| 目录 | `graph-tests/enc_cann_ntt/EP04-encaps-liboqs-cross/` |

| 模式 | wall | tick | c/K vs liboqs |
|------|------|------|---------------|
| CPU | 1.814s | — | max=0 |
| SIM | 121.261s | **812819** | max=0 |

`liboqs_kem_vs_ascendc_verify.py --stage encaps`。禁 npu。
