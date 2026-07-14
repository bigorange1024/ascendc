# AGENTS.md — 索引/路由

> 本文件**只是字典**：告诉 Agent「该去读哪些文档、代码、Rule 与 Skill」，**不写**任何具体工作内容或结论。
> 具体内容一律在下列各自的文件里；本文件只做指路，保持解耦。新会话请按「先读」一节的顺序进入。

## 先读（入口顺序）

| # | 路径 | 作用 |
|---|------|------|
| 1 | [`README.md`](README.md) | 工程目标与顶层目录结构 |
| 2 | [`AGENT_HANDOFF.md`](AGENT_HANDOFF.md) | 办公室 ↔ 家里 Agent 每日交接（当前真相、smoke） |
| 3 | [`qa/INDEX.md`](qa/INDEX.md) · [`qa/TODO.md`](qa/TODO.md) | 近期讨论回忆、遗留事项 |
| 4 | [`.cursor/rules/ascendc-development.mdc`](.cursor/rules/ascendc-development.mdc) | 全仓底线（**始终生效**，含 frozen 禁令、customspec 门禁等） |

## 环境（按运行场景选一）

| 场景 | 路径 |
|------|------|
| 本机 WSL / 无 NPU 复现 | [`docs/engineering/环境复现与开发指南.md`](docs/engineering/环境复现与开发指南.md) |
| **Cursor Cloud VM**（非 WSL）启动/运行坑与 SIM 绕过 | [`Cursor-Cloud环境说明.md`](Cursor-Cloud环境说明.md) |
| 内核超时 vs NTT 性能定标 | [`docs/engineering/内核计算超时与性能定标.md`](docs/engineering/内核计算超时与性能定标.md) |

## Rule 与 Skill

| 路径 | 作用 |
|------|------|
| [`.cursor/rules/ascendc-development.mdc`](.cursor/rules/ascendc-development.mdc) | 唯一 Rule（变更须用户确认） |
| [`.cursor/skills/INDEX.md`](.cursor/skills/INDEX.md) | 场景 Skill 列表与触发符号（`$…$`→impl-spec、`【】`→pre-research、`#…#`→delivery） |

## 目录索引（写码/查基线前先读对应 INDEX）

| 路径 | 覆盖 |
|------|------|
| [`docs/INDEX.md`](docs/INDEX.md) | 项目文档与何时阅读（specs/ engineering/ notes/ reports/） |
| [`examples/INDEX.md`](examples/INDEX.md) · [`examples/incubating/INDEX.md`](examples/incubating/INDEX.md) · [`examples/stable/INDEX.md`](examples/stable/INDEX.md) | 算子研究/定型代码 |
| [`ascendc-tests/INDEX.md`](ascendc-tests/INDEX.md) | 平台功能探针 |
| [`library/INDEX.md`](library/INDEX.md) | 外部资料 · [`library/shared/INDEX.md`](library/shared/INDEX.md) 共用代码 |
| [`library/documents/CANN-AscendC算子开发接口参考-查阅索引.md`](library/documents/CANN-AscendC算子开发接口参考-查阅索引.md) | **写/改 AscendC 前必查**的 API 索引 |

## 关键定稿 / 治理

| 主题 | 路径 |
|------|------|
| 研究路线与 frozen 治理（进门读判决书、出门不带码） | [`docs/notes/研究路线与frozen治理.md`](docs/notes/研究路线与frozen治理.md) |
| ML-KEM 正向 NTT 设备实现 | [`docs/notes/MLKEM-NTT-向量与标量实现指南.md`](docs/notes/MLKEM-NTT-向量与标量实现指南.md) |
| 已关闭路线索引 | [`ascendc-tests/frozen/INDEX.md`](ascendc-tests/frozen/INDEX.md) · `examples/frozen/`（各 `FROZEN.md`） |
