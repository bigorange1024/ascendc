# docs/specs — 规格与实施说明

定型前的**研究计划**、接口约定、**baseline-registry**（交付侧基准计算登记表）等。

| 子目录 / 文件 | 何时阅读 |
|---------------|----------|
| [ascendc/INDEX.md](ascendc/INDEX.md) | AscendC 多核 tiling、融合算子 Host 配参规范 |
| [fips203-mlkem1024-pke-keygen-baseline-registry.md](fips203-mlkem1024-pke-keygen-baseline-registry.md) | Alg.13 PKE KeyGen 交付 golden / KAT 计算块登记（2026-07-20 补登记） |
| [fips203-mlkem1024-pke-encrypt-baseline-registry.md](fips203-mlkem1024-pke-encrypt-baseline-registry.md) | Alg.14 Encrypt 交付 golden / KAT 计算块登记 |
| [fips203-mlkem1024-pke-decrypt-baseline-registry.md](fips203-mlkem1024-pke-decrypt-baseline-registry.md) | Alg.15 PKE Decrypt 交付 golden / KAT 计算块登记（2026-07-20 补登记） |
| [fips203-mlkem1024-kem-keygen-baseline-registry.md](fips203-mlkem1024-kem-keygen-baseline-registry.md) | Alg.19 KEM KeyGen 交付 golden / KAT 计算块登记 |
| [fips203-mlkem1024-kem-encaps-baseline-registry.md](fips203-mlkem1024-kem-encaps-baseline-registry.md) | Alg.20 KEM Encaps 交付 golden / KAT 计算块登记 |
| [fips203-mlkem1024-kem-decaps-baseline-registry.md](fips203-mlkem1024-kem-decaps-baseline-registry.md) | Alg.21 KEM Decaps 交付 golden / KAT 计算块登记 |
| [fips203-mlkem512-parameter-card.md](fips203-mlkem512-parameter-card.md) | **ML-KEM-512 P0 参数卡（草案待锁）**：S-1 单 cube / T-B2 polyvec4 / `-k2` / CBD‑η3 |
| [fips203-mlkem768-parameter-card.md](fips203-mlkem768-parameter-card.md) | **ML-KEM-768 P0 参数卡**（已锁：T-B / `-k3` / CT / PKE+KEM exp） |
| [fips203-mlkem768-p1-gap-and-cases.md](fips203-mlkem768-p1-gap-and-cases.md) | **ML-KEM-768 P1** 缺项对照（补缺图）与必建用例表（已定稿） |
| [fips203-mlkem768-pke-keygen-baseline-registry.md](fips203-mlkem768-pke-keygen-baseline-registry.md) | Alg.13（k=3）registry；E13 incubating CPU/SIM 已验证（2026-07-26） |
| [fips203-mlkem768-pke-encrypt-baseline-registry.md](fips203-mlkem768-pke-encrypt-baseline-registry.md) | Alg.14（k=3）registry；E14 incubating CPU/SIM 已验证（2026-07-26） |
| [fips203-mlkem768-pke-decrypt-baseline-registry.md](fips203-mlkem768-pke-decrypt-baseline-registry.md) | Alg.15（k=3）registry；E15 incubating CPU/SIM 已验证（2026-07-26） |
| [fips203-mlkem768-kem-keygen-baseline-registry.md](fips203-mlkem768-kem-keygen-baseline-registry.md) | Alg.19（k=3）registry；E19 incubating CPU/SIM + AscendC-only roundtrip 已验证（2026-07-26） |
| [fips203-mlkem768-kem-encaps-baseline-registry.md](fips203-mlkem768-kem-encaps-baseline-registry.md) | Alg.20（k=3）registry；E20 incubating CPU/SIM + AscendC-only roundtrip 已验证（2026-07-26） |
| [fips203-mlkem768-kem-decaps-baseline-registry.md](fips203-mlkem768-kem-decaps-baseline-registry.md) | Alg.21（k=3）registry；E21/E21ct incubating accept/reject CPU/SIM + AscendC-only roundtrip 已验证（2026-07-26） |

---

## baseline-registry 约定

- 路径：`docs/specs/<主题>-baseline-registry.md`
- 登记已验证的 API、LUT、数据来源；生成 golden 的计算内核**不得**超出登记表。
- 缺项时 Agent **须停下**请用户补全（见 Rule）。
- **晋级硬卡点**：执行 `#交付#` / `#验收#`（`exp-*` → `stable-*`）**之前**须已定稿并进本 INDEX（见 **ascendc-delivery** Skill）。

---

## 维护

新增 spec 或 registry → 在本表登记。
