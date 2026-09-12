# EP01 FEEDBACK — Encaps Host 壳（SIM-only）

| 项 | 值 |
|----|-----|
| 状态 | **PASS** |
| 目录 | `graph-tests/enc_cann_ntt/EP01-encaps-host-skel/` |

| 模式 | wall | tick | 门禁 |
|------|------|------|------|
| CPU | 0.842s | — | c/K 长度 + H/G/c_stub max=0 |
| SIM | 0.833s | 2940 | 同上 |

Host：tiny_sha3 H/G；桩 Encrypt；AIV `enc_pack_stub` 证 SIM 壳。禁 npu。
