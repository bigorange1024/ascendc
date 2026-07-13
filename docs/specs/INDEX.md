# docs/specs — 规格与实施说明

定型前的**研究计划**、接口约定、**baseline-registry**（交付侧基准计算登记表）等。

| 子目录 / 文件 | 何时阅读 |
|---------------|----------|
| [ascendc/INDEX.md](ascendc/INDEX.md) | AscendC 多核 tiling、融合算子 Host 配参规范 |
| [fips203-mlkem1024-pke-encrypt-baseline-registry.md](fips203-mlkem1024-pke-encrypt-baseline-registry.md) | Alg.14 Encrypt 交付 golden / KAT 计算块登记 |
| [fips203-mlkem1024-kem-keygen-baseline-registry.md](fips203-mlkem1024-kem-keygen-baseline-registry.md) | Alg.19 KEM KeyGen 交付 golden / KAT 计算块登记 |

---

## baseline-registry 约定

- 路径：`docs/specs/<主题>-baseline-registry.md`
- 登记已验证的 API、LUT、数据来源；生成 golden 的计算内核**不得**超出登记表。
- 缺项时 Agent **须停下**请用户补全（见 Rule）。

---

## 维护

新增 spec 或 registry → 在本表登记。
