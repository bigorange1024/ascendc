# STATUS — exp-fips203-mlkem-pke-decrypt-k3

| 项 | 状态 |
|----|------|
| 阶段 | **customspec 已完成**（待写 incubating 实现） |
| 波次 | W4a / E15 |
| CPU | — |
| SIM | — |
| customspec | [`exp-fips203-mlkem-pke-decrypt-k3-实现方案-customspec.pdf`](exp-fips203-mlkem-pke-decrypt-k3-实现方案-customspec.pdf) |
| 参数卡 | [fips203-mlkem768-parameter-card.md](../../../../../docs/specs/fips203-mlkem768-parameter-card.md) |

## 锁定语义

FIPS 203 **Alg.15 PKE Decrypt**（ML-KEM-768，`k=3`），对齐参数卡 §3.2 D15：

| 项 | 值 |
|----|----|
| I/O | `dk_pke.bin` **1152B** + `c.bin` **1088B** → `m.bin` **32B** |
| Launch | **1**：fused `MIX blockDim=1` |
| prep | ByteDecode₁₀/₄/₁₂ 标量；Decompress₁₀/₄ 向量 |
| compute | NTT/INTT k=3；AIV **2+1**；tail Compress₁ + ByteEncode₁ |

## 下一步

- 按 customspec 从活跃 D15 k3 探针复制并自包含到本 exp；不得从 `**/frozen/**` 复制。
- 本阶段不建 `examples/stable/ml-kem/ml-kem-768/`。
