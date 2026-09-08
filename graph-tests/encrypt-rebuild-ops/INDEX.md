# encrypt-rebuild-ops — 任务 / 反馈 / 日志运营目录

> **用途**：主控预先写好、再下发给 Subagent 的任务队列；与实现代码目录分离。  
> **NPU**：主控独占 910B3 `ASCEND_DEVICE_ID=0`；与本机 SIM **双轨并行**（不等 SIM 收官才上板）。  
> **知识**：[`Encrypt-cannbot-rebuild-kb.md`](../../docs/notes/Encrypt-cannbot-rebuild-kb.md) · DAG [`rg-encrypt-cannbot-rebuild.yaml`](../../docs/rg-encrypt-cannbot-rebuild.yaml)  
> **积木**：[`Encrypt-cannbot-rebuild-capability-inventory.md`](../../docs/notes/Encrypt-cannbot-rebuild-capability-inventory.md)  
> **工作模式**：[`Encrypt-cannbot-rebuild-work-mode.md`](../../docs/notes/Encrypt-cannbot-rebuild-work-mode.md)

## 布局

| 路径 | 谁写 | 内容 |
|------|------|------|
| [`QUEUE.md`](QUEUE.md) | 主控 | 总序、依赖、派发状态 |
| [`COMMON.md`](COMMON.md) | 主控 | 全局禁令、cannbot 命令、回报格式 |
| `tasks/<ID>/TASK.md` | 主控 | 单刀任务书（下发依据） |
| `tasks/<ID>/FEEDBACK.md` | Subagent | 短回报；主控可批注 |
| `tasks/<ID>/logs/` | Subagent | 粘贴关键日志片段 / 审计 JSON 副本 |
| `tasks/<ID>/STATUS.link.md` | Subagent | 指向实现目录 `STATUS.md` 的相对链接说明 |
| 实现目录 | Subagent | 见各 TASK「代码目录」：`graph-tests/toys|enc_related|bricks/…` |

## 派发规则

1. 严格按 `QUEUE.md` 依赖；未满足依赖的刀 **不得派发**。  
2. 同一时刻 **禁止** 两路 SIM；可串行多刀。  
3. Subagent **只读** KB/DAG；**禁止**改 `docs/rg-*.yaml` / KB。  
4. 主控回收后刷新 KB（P*/X*）与 DAG，再改本 QUEUE 状态。

## 当前快照（2026-09-08）

- **已双绿**：T01–T12 / N00–N02；T13 本机绿、NPU 修版重跑
- **NPU**：远端 nohup 循环占卡
- **本机**：T14 设备 Â+ŷ 半链拼装
