# docs/research — 调研草稿 / 未定稿笔记

**性质**：仍在调研、方案未锁定、或尚未按模板定稿的过程性笔记。  
**不是**交付验收依据；**不是** frozen 源码替代物。

| 目录 | 职责 |
|------|------|
| **本目录 `research/`** | 预研草稿、路线比较、未关闭问题；允许粗糙、可废弃 |
| [`notes/`](../notes/INDEX.md) | **定稿**技术总结（原理优先；见 [写作模板](../notes/技术总结写作模板.md)） |
| [`specs/`](../specs/INDEX.md) | 研究计划、baseline-registry、customspec 配套 |
| `qa/` | 按日讨论纪要与 TODO |

**晋级**：草稿结论稳定后 → 按模板写入 `notes/`（或 `specs/`），本目录条目可删或标「已迁出」。

**历史**：2026-06-18 曾废除整目录并迁入 `notes/`；**2026-07-15** 用户要求恢复，用于与定稿分离的调研区。

---

## 当前条目

| 文件 | 说明 |
|------|------|
| [教材KEM实机测量清单.md](教材KEM实机测量清单.md) | 借入实机测 KEM：表 A 14 档 + 表 B 16 档脚本对齐；多 launch 测准口径 |
| [MLKEM-512-从0到exp完整实现计划.md](MLKEM-512-从0到exp完整实现计划.md) | 真 k=2 + **单 AI Core**；**有条件完成至 incubating**（W0–W4+glue；glue-c `r←η1=3`）；用语：缺项/补缺；权威卡见 `docs/specs/fips203-mlkem512-parameter-card.md` |
| [MLKEM-768-从0到exp完整实现计划.md](MLKEM-768-从0到exp完整实现计划.md) | 真 k=3 从 0→exp；**有条件完成至 incubating**（W0–W4+glue）；权威表见 `docs/specs/fips203-mlkem768-*` |
| [教材与调研文档写作指导.md](教材与调研文档写作指导.md) | **写文档口味**：文风、禁翻译腔/工程黑话、Ascend 字体、图示与指标；2026-08-20 刷新 |
| [从已验证能力到合法派生-面向Agent预研的形式方法教材草案.tex](从已验证能力到合法派生-面向Agent预研的形式方法教材草案.tex) / [PDF](从已验证能力到合法派生-面向Agent预研的形式方法教材草案.pdf) | **专题唯一长文**（无日期、持续修订）：第6–7章 1024 复盘/前瞻；第8章 ML-KEM-768；第9章对照实验；**第10章总结**（主要结论、未证命题；2026-08-19 文风修订） |
| [形式语言与自动机预研讨论纪要.md](形式语言与自动机预研讨论纪要.md) | **专题唯一纪要**（持续刷新）：讨论过程与约定；正文以教材草案 PDF 为准 |
| [2026-07-15-T19a-KEM-Encaps-device要点.md](2026-07-15-T19a-KEM-Encaps-device要点.md) | Alg.20 Encaps device（T19a）锁定要点与验收；详案仍以探针 INTEGRATION_PLAN 为准 |
| [2026-07-24-Decaps-correctness与CT五指标对照.md](2026-07-24-Decaps-correctness与CT五指标对照.md) | Alg.21 Decaps：**五指标** A correctness vs B CT 路径（精简对照；不比 token） |
