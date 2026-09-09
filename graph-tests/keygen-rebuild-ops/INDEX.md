# keygen-rebuild-ops — PKE/KEM KeyGen 任务运营目录

> **用途**：主控任务队列；与实现代码目录分离。  
> **知识**：[`ascendc-engineering-kb.md`](../../docs/notes/ascendc-engineering-kb.md) · DAG [`rg-ascendc-engineering.yaml`](../../docs/rg-ascendc-engineering.yaml) · viz [`rg-ascendc-engineering.viz.html`](../../docs/rg-ascendc-engineering.viz.html)  
> **积木**：[`KeyGen-cannbot-rebuild-capability-inventory.md`](../../docs/notes/KeyGen-cannbot-rebuild-capability-inventory.md)  
> **工作模式**：[`KeyGen-cannbot-rebuild-work-mode.md`](../../docs/notes/KeyGen-cannbot-rebuild-work-mode.md) · [`COMMON.md`](COMMON.md)  
> **前序**：Encrypt / Decrypt 战役已收口（只读继承反卡死）

## 布局

| 路径 | 谁写 | 内容 |
|------|------|------|
| [`QUEUE.md`](QUEUE.md) | 主控 | 总序、派发状态 |
| [`rg-keygen-dag.html`](rg-keygen-dag.html) | — | **跳转** → `docs/rg-ascendc-engineering.viz.html` |
| [`MATRIX.md`](MATRIX.md) | 主控 | NPU 加压证据表 |
| [`NPU_LIVE.md`](NPU_LIVE.md) | 主控 | 人话进度 |
| [`COMMON.md`](COMMON.md) | 主控 | 全局禁令 |
| `tasks/<ID>/TASK.md` | 主控 | 单刀任务书 |
| `tasks/<ID>/FEEDBACK.md` | Subagent / 主控 | 回报 |
| 实现目录 | Subagent | `graph-tests/kg_related/RB-K*` |

## 当前快照（2026-09-09）

- 战役 **启动**；S0 拓扑已锁（KB §B2/B3）  
- 下一刀：**KGR-P01** prep 壳（编码）  
- 云机：默认未开；上板前请用户 bootstrap  
