# encrypt-rebuild-ops — 任务 / 反馈 / 日志运营目录

> **用途**：主控任务队列；与实现代码目录分离。  
> **知识**：[`Encrypt-cannbot-rebuild-kb.md`](../../docs/notes/Encrypt-cannbot-rebuild-kb.md) · [**反卡死拓扑总结（暂行）**](../../docs/notes/MIX-Encrypt-Encaps-反卡死拓扑技术总结.md) · DAG [`rg-encrypt-cannbot-rebuild.yaml`](../../docs/rg-encrypt-cannbot-rebuild.yaml)  
> **积木**：[`Encrypt-cannbot-rebuild-capability-inventory.md`](../../docs/notes/Encrypt-cannbot-rebuild-capability-inventory.md)  
> **工作模式**：[`Encrypt-cannbot-rebuild-work-mode.md`](../../docs/notes/Encrypt-cannbot-rebuild-work-mode.md) · [`COMMON.md`](COMMON.md) · [`RHYTHM.md`](RHYTHM.md)

## 布局

| 路径 | 谁写 | 内容 |
|------|------|------|
| [`QUEUE.md`](QUEUE.md) | 主控 | 总序、依赖、派发状态 |
| [`COMMON.md`](COMMON.md) | 主控 | 全局禁令、cannbot、**反卡死检查单入口** |
| `tasks/<ID>/TASK.md` | 主控 | 单刀任务书 |
| `tasks/<ID>/FEEDBACK.md` | Subagent | 短回报；主控批注 |
| `tasks/<ID>/logs/` | Subagent | 日志 / sync_audit |
| 实现目录 | Subagent | `graph-tests/toys|bricks|enc_related/…` |

## 当前快照（2026-09-08 收口）

- **Q-ULT answered**：T22–T24 NPU 双绿；T24×30 不挂  
- **指导后续**：反卡死笔记 §5 + DAG `C-ANTI-HANG-CHECKLIST`  
- **可选**：设备 Decaps；NPU 可 T06 sticky
