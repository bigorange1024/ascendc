# docs — 项目文档索引

**本 INDEX**：本项目撰写文档的入口；讨论见 `qa/INDEX.md`；算子语义见 `examples/INDEX.md`。

---

## 子目录

| 子目录 | INDEX | 用途 |
|--------|-------|------|
| [research/](research/INDEX.md) | **调研草稿**（未定稿；可废弃） |
| [specs/](specs/INDEX.md) | 研究计划、baseline-registry、实施说明 |
| [engineering/](engineering/INDEX.md) | 环境复现、工具链、Agent 清单 |
| [notes/](notes/INDEX.md) | **定稿技术总结**（原理层） |
| [reports/](reports/INDEX.md) | 汇报、论文、正式报告（含 [方法论精髓简报](reports/Agent预研形式方法-方法论精髓与效果-简报.pdf)） |

> **`research/` 与 `notes/`**：草稿 → research；结论稳定后再按 [技术总结写作模板](notes/技术总结写作模板.md) 写入 notes。  
> 历史：2026-06-18 曾整目录迁入 notes；**2026-07-15** 恢复 research 作调研区。

---

## 快速入口

| 场景 | 阅读 |
|------|------|
| **Encrypt/l18 卡死推理图谱** | [`rg-encrypt-l18.yaml`](rg-encrypt-l18.yaml)（工具：`thirdparty/reasoning-graph-skill/`） |
| **Decrypt fused 卡死推理图谱** | [`rg-decrypt-fused.yaml`](rg-decrypt-fused.yaml)（试验场 [`graph-tests/decrypt/`](../graph-tests/decrypt/INDEX.md)） |
| 新 Agent / 新机器 | [engineering/环境复现与开发指南.md](engineering/环境复现与开发指南.md) §12–§14；探针超时见 [engineering/内核计算超时与性能定标.md](engineering/内核计算超时与性能定标.md)；额度排程见 [engineering/Cloud-Agent额度与验收分层.md](engineering/Cloud-Agent额度与验收分层.md) |
| **写调研草稿** | [research/INDEX.md](research/INDEX.md) |
| **ML-KEM-512（有条件完成至 incubating）** | [specs/fips203-mlkem512-parameter-card.md](specs/fips203-mlkem512-parameter-card.md) · [P1 表](specs/fips203-mlkem512-p1-gap-and-cases.md) · [计划](research/MLKEM-512-从0到exp完整实现计划.md) · [当日纪要](../qa/2026-07/2026-07-27-768收尾复盘与文档刷新.md) |
| **ML-KEM-768（有条件完成至 incubating）** | [specs/fips203-mlkem768-parameter-card.md](specs/fips203-mlkem768-parameter-card.md) · [P1 用例表](specs/fips203-mlkem768-p1-gap-and-cases.md) · [计划](research/MLKEM-768-从0到exp完整实现计划.md) · [教材第8章](research/从已验证能力到合法派生-面向Agent预研的形式方法教材草案.pdf) · [当日纪要](../qa/2026-07/2026-07-27-768收尾复盘与文档刷新.md) |
| **写技术总结** | [notes/技术总结写作模板.md](notes/技术总结写作模板.md)；归档约定见 [ascendc-development.mdc](../.cursor/rules/ascendc-development.mdc) |
| **frozen 治理** | [notes/研究路线与frozen治理.md](notes/研究路线与frozen治理.md) |
| **ML-KEM NTT** | [notes/MLKEM-NTT-实现总结.md](notes/MLKEM-NTT-实现总结.md) + [向量与标量指南](notes/MLKEM-NTT-向量与标量实现指南.md) |
| **NTT+内积融合** | [notes/F203-2s1e-NTT内积UB融合技术总结.md](notes/F203-2s1e-NTT内积UB融合技术总结.md) |
| **内积 4×4×1** | [notes/F203-innerproduct-k4-技术总结.md](notes/F203-innerproduct-k4-技术总结.md) |
| merged_kyber MIX（frozen） | [notes/F203-merged-kyber-MIX路线技术总结.md](notes/F203-merged-kyber-MIX路线技术总结.md) |
| NTT 内 Matmul 废弃 | [notes/NTT-Matmul路线废弃说明.md](notes/NTT-Matmul路线废弃说明.md) |
| DataCopy / TQue | [notes/ascendc-DataCopy与数据搬运知识库.md](notes/ascendc-DataCopy与数据搬运知识库.md)、[TQue 知识库](notes/ascendc-TQue与Pipe框架知识库.md) |
| 多核 MatMul tiling | [notes/AscendC-多核MatMul-tiling技术总结.md](notes/AscendC-多核MatMul-tiling技术总结.md) |

---

## 维护

增删 `docs/` 文档 → 更新对应子目录 `INDEX.md` 与本表。
