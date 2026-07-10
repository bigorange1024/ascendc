# STATUS — fix-f203-alg21-kem-decaps-device-k4

FIPS 203 **Algorithm 21 `ML-KEM.Decaps(dk, c)`** — **无 vendor 设备主线**（规划中）。

| 项 | 值 |
|---|---|
| **阶段** | **待开工**（2026-07-10） |
| **后继自** | [`fix-f203-alg21-kem-decaps-correctness-k4`](../fix-f203-alg21-kem-decaps-correctness-k4/)（CPU+SIM PASS；vendor frozen G4+G5；SIM 默认 2-session） |
| **目标** | **T19b/c**：Phase-E 对齐 stable Encrypt；Phase-D 对齐 [`stable-fips203-mlkem-pke-decrypt-k4`](../../examples/stable/stable-fips203-mlkem-pke-decrypt-k4/) fused |
| **笔记** | [`docs/notes/F203-KEM-Alg21-Decaps设备全链与SIM单session技术总结.md`](../../docs/notes/F203-KEM-Alg21-Decaps设备全链与SIM单session技术总结.md) |

**当前**：`run.sh` exit **2**。验收默认仍走 **correctness-k4**。
