# STATUS — exp-fips203-mlkem-pke-encrypt-k3

| 项 | 状态 |
|----|------|
| 阶段 | **customspec 已完成**（待写 incubating 实现） |
| 波次 | W4a / E14 |
| CPU | — |
| SIM | — |
| customspec | [`exp-fips203-mlkem-pke-encrypt-k3-实现方案-customspec.pdf`](exp-fips203-mlkem-pke-encrypt-k3-实现方案-customspec.pdf) |
| 参数卡 | [fips203-mlkem768-parameter-card.md](../../../../../docs/specs/fips203-mlkem768-parameter-card.md) |

## 锁定语义

FIPS 203 **Alg.14 PKE Encrypt**（ML-KEM-768，`k=3`），对齐参数卡 §3.2 D14：

| 项 | 值 |
|----|----|
| I/O | `ek_pke.bin` **1184B** + `m.bin` 32B + `coins.bin` 32B → `c.bin` **1088B** |
| Launch | **2**：prep `AIV_ONLY blockDim=2` → compute+tail `MIX blockDim=1` |
| prep | Â **[9,256] int32**，双 AIV **5+4**；`re` **[7,256]**（`r[3]‖e1[3]‖e2[1]`） |
| compute | NTT(`r`) k=3；INTT 真 batch4；`du/dv=10/4`；`c1=960B`、`c2=128B` |

## 下一步

- 按 customspec 从活跃 D14 k3 探针复制并自包含到本 exp；不得从 `**/frozen/**` 复制。
- 本阶段不建 `examples/stable/ml-kem/ml-kem-768/`。
