# 自包含与设备全链约束 — fix-f203-alg21-kem-decaps-device-k4

| 项 | 约定 |
|----|------|
| **PKE** | 编译期引用 `examples/stable` Encrypt/Decrypt；**无** `vendor/`、**不** rsync frozen |
| **KEM** | Alg.18 的 G / FO 嵌在 Encrypt prep / pack；中间态默认不落盘 |
| **Phase-E-only** | Host 可灌 `m'`/`h`/`z`/`ek`/`c` 做分段验收；**不**用 Host 算最终 `K` |
| **FO** | 设备完成；合法 + 拒绝路径均须对拍 |
| **对照** | correctness 仅 oracle；实现不抄 frozen 源码 |
| **SIM 库** | CPU **单库**；SIM 现 **双库 + 2-session**（同名头 precompile 隔离失败）；**T2** 目标单库/单 session（Cloud） |
