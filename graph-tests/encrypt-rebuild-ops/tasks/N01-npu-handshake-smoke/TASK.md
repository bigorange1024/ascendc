# N01 — NPU 冒烟：复跑握手刀

| 字段 | 值 |
|------|-----|
| 状态 | **wait_npu**（他 Agent 占用；任务书预写） |
| DAG | `D-EXP-N01` |
| 代码目录 | 复用 T01 或 T03 已 PASS 目录（不新建业务核） |
| 运营目录 | `…/tasks/N01-npu-handshake-smoke/` |
| 墙钟 | ≤ 25 min（上机） |

继承 COMMON · NPU 节。

## 目标

在 **空闲 NPU** 上复跑 T01（优先）或 T03：`bash run.sh -r npu …`（按本仓 runtime_env / CANNLab 约定）。  
验收：**不挂** + 退出 0；记录 device id、是否污染恢复。

## 前置

- T01 SIM PASS  
- 用户确认 NPU 空闲；`ASCEND_DEVICE_ID`（CANNLab=0）  
- 读 KB §X3

## 非目标

改 FSM；并行占卡；与他 Agent 抢同一 device。

## 验收

- NPU 日志入 `logs/`  
- FEEDBACK：`npu: ok|hang|pollute`  
- 若 hang：按 deadlock-triage 写假设，**停**，交主控

## 依赖

T01 PASS + NPU 空闲授权。
