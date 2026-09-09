# TASK-NPU-AB — 关 TRACE vs 开 TRACE-DC 对照（同卡连续）

**交付模式（强制）**：`scripts/cannlab/run_npu_ab_nohup.sh` — **禁止** subagent 挂长 SSH。  
**主机**：`SSH_HOST=cannlab-npu-1`（`100.97.98.72`）；本机另开 `agent_link_keepalive.sh`。

## 一键

```bash
export SSH_HOST=cannlab-npu-1
nohup bash scripts/cannlab/agent_link_keepalive.sh >/tmp/cannlab_keepalive.log 2>&1 &
bash scripts/cannlab/run_npu_ab_nohup.sh submit
# 短连轮询即可；Ctrl-C 不影响远端
bash scripts/cannlab/run_npu_ab_nohup.sh poll
bash scripts/cannlab/run_npu_ab_nohup.sh fetch
```

远程工作树：`/mnt/workspace/ascendc`（encaps 已含 DataCopy `FusedTraceMark`）。

## NPU-A → NPU-B（脚本内串联）

| 阶段 | TRACE | 轮次 | timeout |
|------|-------|------|---------|
| A | 关（unset） | ×12 | 240s |
| B | `F203_L18_TRACE=1` | ×12 | 240s |

首轮 FORCE rebuild；SOC `Ascend910B3`；`ASCEND_DEVICE_ID=0` `CANNLAB=1`。

## 判读 → FEEDBACK-NPU-AB.md

| A | B | 推论 |
|---|---|------|
| 多轮 PASS 后挂 | 挂率更高/首轮挂 | TRACE-DC **抬挂率** |
| 挂率相近 | 挂时 B 有 16/16 | 探针主要修观测；真挂独立 |
| A 也 0 PASS | — | 污染主导；须日后干净卡复验 |

## 禁

- 长 SSH；并行第二路 NPU；本刀改码；擅自 commit/push  
- 跑完提醒：**请控制台关机**  
