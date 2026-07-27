# 自包含与设备全链约束 — pass-fix-f203-alg21-kem-decaps-device-ct-k2

| 项 | 约定 |
|----|------|
| **PKE** | 编译期引用活跃 D14/D15 k2 Encrypt/Decrypt；**无** `vendor/`、**不** rsync frozen |
| **KEM** | Alg.18 的 G 嵌 Encrypt prep；FO 设备完成（SIM `l18_l19` 同核 pack+FO；CPU `pack_fo`） |
| **Phase-E-only** | Host 可灌 `m'`/`h`/`z`/`ek`/`c` 做分段验收；**不**用 Host 算最终 `K` |
| **FO** | 设备完成；合法 + 拒绝路径均须对拍 |
| **对照** | correctness 仅 oracle；实现不抄 frozen 源码 |
| **SIM 库** | CPU/SIM **单** `libascendc_kernels_*.so`；本 CT 探针默认 `decaps_2session`；`decaps_1session` 仅作排障覆盖；shim 见 `scripts/prepare_dec_shim.sh`（生成物 gitignore） |
