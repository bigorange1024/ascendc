# INTEGRATION_PLAN — fix-f203-alg21-kem-decaps-device-k4

**定位**：Alg.21 KEM Decaps **无 vendor** 设备主线；设备 FO + stable 对齐的 PKE Decrypt/Encrypt 段。

**基线对照**：[`fix-f203-alg21-kem-decaps-correctness-k4`](../fix-f203-alg21-kem-decaps-correctness-k4/)

## T19b/c 要点

| 子项 | 内容 |
|------|------|
| **T19b** | Phase-E（re-encrypt）接 stable/pass-fix Encrypt 布局，**无** frozen G5 vendor |
| **T19c** | Phase-D 接 stable Decrypt **fused** 或等价 g4 入口，**无** frozen G4 2-launch vendor |
| **FO** | 保持设备 Fujisaki–Okamoto；SIM 单 session 为 T2 后续项 |

## 上游 input

| 文件 | 来源 |
|------|------|
| `dk_kem.bin` | alg19 device-k4（或 correctness 过渡） |
| `c.bin` | alg20 device-k4（或 correctness 过渡） |
