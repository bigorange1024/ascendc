# 自包含与设备全链约束 — pass-fix-f203-alg21-kem-decaps-device-ct-k4

| 项 | 约定 |
|----|------|
| **PKE** | 编译期引用 `examples/stable` Encrypt/Decrypt；**无** `vendor/`、**不** rsync frozen |
| **KEM** | Alg.18 的 G 嵌 Encrypt prep；FO 设备完成（SIM 过渡独立 `fo_only`；CPU `pack_fo`） |
| **Phase-E-only** | Host 可灌 `m'`/`h`/`z`/`ek`/`c` 做分段验收；**不**用 Host 算最终 `K` |
| **FO** | 设备完成；合法 + 拒绝路径均须对拍 |
| **对照** | correctness 仅 oracle；实现不抄 frozen 源码 |
| **SIM 库** | CPU/SIM **单** `libascendc_kernels_*.so`；默认 `decaps_1session`；shim 见 `scripts/prepare_dec_shim.sh`（生成物 gitignore） |
