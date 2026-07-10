# STATUS — fix-f203-alg20-kem-encaps-device-k4

FIPS 203 **Algorithm 20 `ML-KEM.Encaps(ek)`** — **无 vendor 设备主线**（规划中）。

| 项 | 值 |
|---|---|
| **阶段** | **待开工**（2026-07-10） |
| **后继自** | [`fix-f203-alg20-kem-encaps-correctness-k4`](../fix-f203-alg20-kem-encaps-correctness-k4/)（CPU+SIM PASS，vendor frozen G5） |
| **目标** | **T19a**：Alg.14 Encrypt 改接 [`stable-fips203-mlkem-pke-encrypt-k4`](../../examples/stable/stable-fips203-mlkem-pke-encrypt-k4/) 或等价 pass-fix device 布局 |
| **公钥 input** | `ek_kem.bin` ← alg19 **device-k4**（或过渡期 correctness） |

**当前**：`run.sh` exit **2**。round-trip 默认仍走 **correctness-k4**。
