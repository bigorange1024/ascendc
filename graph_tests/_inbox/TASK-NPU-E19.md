# TASK-NPU-E19 — 实机 EARLY 入口 TRACE

**deadline_min**: 45  
**max_retries**: 1  
**abort_on**: [timeout, no_progress, scope_breach]

## 目标

在 CANNLab `developer@cannlab-npu:2222`（ts `100.81.136.67`）上跑 **E19** 多轮，回答：实机入口标是否可见；E19 本身是否粘性挂。

## 连接（本机已有 SOCKS）

```bash
SSH=(ssh -o ServerAliveInterval=15 -o ProxyCommand="nc -X 5 -x 127.0.0.1:1055 %h %p" -o StrictHostKeyChecking=accept-new -i ~/.ssh/cannlab -p 2222 developer@cannlab-npu)
```

仓库应已在 `/mnt/workspace/ascendc` 的 `cursor/kem-2launch-sticky-1534` 且含 `toy-e19-early-entry-trace`。若缺则 `git pull`。

## 实验步骤

1. `source` Ascend set_env；`ASCEND_DEVICE_ID=0` `CANNLAB=1`
2. `cd .../toy-e19-early-entry-trace`
3. `TOY_ROUNDS=12 KERNEL_COMPUTE_BUDGET_SEC=600`（或用例默认）  
   `bash run.sh -r npu -v Ascend910B3`
4. 日志落到 `/mnt/workspace/npu_e19_$(date +%Y%m%d_%H%M%S)/run.log`，并 tee 关键线索
5. 抓：`REPORT:` / `[e19-trace]` / `SUCCESS` / `TIMEOUT` / Host `100/111`

**TIMEOUT**：整段墙钟紧；若单进程 12 轮，预算 ≥ 编译+运行；卡死用外层 `timeout 900` 包一层即可。

## 反馈判读（写进 FEEDBACK）

| 结果 | effect |
|------|--------|
| 12 轮绿且见入口槽（0/15） | support：E19 实机可活；H-E3 对「纯入口 stub」弱；下一刀 Encaps Mark-before-Prefix |
| 中途挂且入口槽也无 | support H-E3 |
| 中途挂但入口槽有 | 非 H-E3；对照 Encaps |

## 交付

- `/workspace/graph_tests/_outbox/FEEDBACK-NPU-E19.md`
- 日志副本：`/opt/cursor/artifacts/npu_e19_*.log`（scp 回本机）
- **不要**改 Encaps FSM；不要 commit/push；不要并行第二路 NPU
- 跑完在 FEEDBACK 提醒：**请用户控制台关机**

## 白名单

远程：只跑 E19 目录 + 写 `/mnt/workspace/npu_e19_*`  
本机：只写 `_outbox/FEEDBACK-NPU-E19.md` + `/opt/cursor/artifacts/`
