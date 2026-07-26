# STATUS — exp-fips203-mlkem-kem-keygen-k3

| 项 | 状态 |
|----|------|
| 阶段 | **P0/P1 目录壳**（无源码 / 无 customspec） |
| 波次 | W4b / E19 |
| CPU | — |
| SIM | — |
| customspec | [`exp-fips203-mlkem-kem-keygen-k3-实现方案-customspec.pdf`](exp-fips203-mlkem-kem-keygen-k3-实现方案-customspec.pdf) |
| 参数卡 | [fips203-mlkem768-parameter-card.md](../../../../../docs/specs/fips203-mlkem768-parameter-card.md) |

## 锁定语义

FIPS 203 **Alg.19 KEM KeyGen**（ML-KEM-768，`k=3`），对齐参数卡 §3.3 D19：

| 项 | 值 |
|----|----|
| I/O | `seed_d.bin` + LUT → `ek_kem.bin` **1184B** + `dk_kem.bin` **2400B** |
| Launch | **2**：prep `AIV_ONLY blockDim=2` → compute+Alg.16 tail `MIX blockDim=1` |
| PKE 主体 | 复用 E13/D13 k3 几何：Â `[9,256]`、polyvec6、Inner 2+1、ByteEncode12 `3×384` |
| KEM 尾 | `dk_kem = dk_pke(1152)‖ek(1184)‖H(ek)(32)‖z(32)`，内嵌第二 launch，无第三 launch |

## 当前状态

- customspec 已生成；下一步按该规格写自包含 incubating 实现。
- 本阶段不建 `examples/stable/ml-kem/ml-kem-768/`。
