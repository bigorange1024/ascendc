# 自包含与设备全链约束 — exp-fips203-mlkem-kem-keygen-k3

本 exp 是 ML-KEM-768 E19 KEM KeyGen incubating 路径：Host 只提供 `seed_d.bin` 与 NTT LUT，设备生成 PKE keypair 并在 compute launch 内拼 `dk_kem`。

| 项 | 约束 |
|----|------|
| I/O | `ek_kem` 1184B，`dk_kem` 2400B |
| PKE 来源 | 活跃 E13/D13 k3 源码副本；禁止从 `frozen/` 取代码 |
| KEM 尾 | `kem/*.hpp` 自研实现，中文注释与代码同轮 |
| 自包含 | `scripts/compute`、`thirdparty`、`scripts/keygen_golden.py` 均为本目录实体文件；运行时不创建 D13 软链 |

禁止：改锁定尺寸、补零凑 k4、增加第三 launch、引用 `examples/stable-768`。
