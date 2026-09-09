# decrypt-rebuild-ops — Decrypt/Decaps 任务运营目录

> **用途**：主控任务队列；与实现代码目录分离。  
> **知识**：[`Decrypt-cannbot-rebuild-kb.md`](../../docs/notes/Decrypt-cannbot-rebuild-kb.md) · [Decrypt 反卡死总结](../../docs/notes/MIX-Decrypt-Decaps-反卡死拓扑技术总结.md) · DAG [`rg-decrypt-cannbot-rebuild.yaml`](../../docs/rg-decrypt-cannbot-rebuild.yaml)  
> **积木**：[`Decrypt-cannbot-rebuild-capability-inventory.md`](../../docs/notes/Decrypt-cannbot-rebuild-capability-inventory.md)  
> **工作模式**：[`Decrypt-cannbot-rebuild-work-mode.md`](../../docs/notes/Decrypt-cannbot-rebuild-work-mode.md) · [`COMMON.md`](COMMON.md) · [`RHYTHM.md`](RHYTHM.md)  
> **前序**：Encrypt 战役已收口 → [`../encrypt-rebuild-ops/`](../encrypt-rebuild-ops/INDEX.md)（只读继承）

## 布局

| 路径 | 谁写 | 内容 |
|------|------|------|
| [`QUEUE.md`](QUEUE.md) | 主控 | 总序、依赖、派发状态 |
| [`MATRIX.md`](MATRIX.md) | 主控 | NPU 加压证据表 |
| [`run_controller.sh`](run_controller.sh) | 本机常驻 | **唯一**真进度：读云日志 → LIVE；缺日志不得显示在跑 |
| [`COMMON.md`](COMMON.md) | 主控 | 全局禁令、cannbot、反卡死入口；预算 **180s** |
| `tasks/<ID>/TASK.md` | 主控 | 单刀任务书 |
| `tasks/<ID>/FEEDBACK.md` | Subagent | 短回报；主控批注 |
| `tasks/<ID>/logs/` | Subagent | 日志 / sync_audit / 设计附件 |
| 实现目录 | Subagent | `graph-tests/dec_related/RB-D*` |

## 当前快照（2026-09-09 16:30）

- 门禁 **Q-DEC / Q-DECAPS / Q-RT-HANG** closed；DG1–DG7 closed  
- **不停手**：D04 NPU×30 → 衔尾 K03×30；中间用例加压默认 ×30  
- 定稿：[Decrypt 反卡死拓扑总结](../../docs/notes/MIX-Decrypt-Decaps-反卡死拓扑技术总结.md)  
- NPU：`scripts/cannlab/which_npu.sh`（勿写死主机名）
