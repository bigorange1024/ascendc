# docs/specs — 规格与实施说明

定型前的**研究计划**、接口约定、**baseline-registry**（交付侧基准计算登记表）等。

| 子目录 / 文件 | 何时阅读 |
|---------------|----------|
| [ascendc/INDEX.md](ascendc/INDEX.md) | AscendC 多核 tiling、融合算子 Host 配参规范 |
| （尚无） | 交付前须有 `<主题>-baseline-registry.md` |

---

## baseline-registry 约定

- 路径：`docs/specs/<主题>-baseline-registry.md`
- 登记已验证的 API、LUT、数据来源；生成 golden 的计算内核**不得**超出登记表。
- 缺项时 Agent **须停下**请用户补全（见 Rule）。

---

## 维护

新增 spec 或 registry → 在本表登记。
