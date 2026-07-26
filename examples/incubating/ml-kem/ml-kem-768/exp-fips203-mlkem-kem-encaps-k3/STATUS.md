# STATUS — exp-fips203-mlkem-kem-encaps-k3

| 项 | 状态 |
|----|------|
| 阶段 | **P0/P1 目录壳**（无源码 / 无 customspec） |
| 波次 | W4b / E20 |
| CPU | — |
| SIM | — |
| customspec | [`exp-fips203-mlkem-kem-encaps-k3-实现方案-customspec.pdf`](exp-fips203-mlkem-kem-encaps-k3-实现方案-customspec.pdf) |
| 参数卡 | [fips203-mlkem768-parameter-card.md](../../../../../docs/specs/fips203-mlkem768-parameter-card.md) |

## 锁定语义

FIPS 203 **Alg.20 KEM Encaps**（ML-KEM-768，`k=3`），对齐参数卡 §3.3 D20：

| 项 | 值 |
|----|----|
| I/O | `ek_kem.bin` **1184B** + `m.bin` 32B + LUT → `c.bin` **1088B** + `K.bin` 32B |
| Launch | SIM **2**：KEM head+prep `AIV_ONLY blockDim=2` → compute+pack `MIX blockDim=1`；CPU 可多分段 |
| KEM 头 | `H(ek)` 与 `G(m‖H(ek))` 在设备侧生成 `K‖r`，`r` 供 Encrypt prep 消费 |
| PKE 主体 | 复用 E14/D14 k3 几何：Â `[9,256]`、`re[7,256]`、INTT batch4、`du/dv=10/4` |

## 当前状态

- customspec 已生成；下一步按该规格写自包含 incubating 实现。
- 本阶段不建 `examples/stable/ml-kem/ml-kem-768/`。
