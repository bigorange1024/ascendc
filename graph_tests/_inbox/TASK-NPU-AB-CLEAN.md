# TASK-NPU-AB-CLEAN — 干净卡（刚 bootstrap）关 TRACE vs TRACE-DC

**卡状态**：用户刚 bootstrap → 视为 **干净冷启**（对比 N18 污染卡）。  
**交付**：`run_npu_ab_nohup.sh`；禁长 SSH；本机 `agent_link_keepalive.sh`。  
**主机**：`which_npu.sh` 自动发现（当前预期 `cannlab-npu-1` = `100.85.76.12`）。

## 一键

```bash
nohup bash scripts/cannlab/agent_link_keepalive.sh >/tmp/cannlab_keepalive.log 2>&1 &
bash scripts/cannlab/run_npu_ab_nohup.sh submit   # A×12 无 TRACE → B×12 TRACE-DC
# timer 短连 poll；勿挂死循环 SSH
bash scripts/cannlab/run_npu_ab_nohup.sh fetch
```

| 阶段 | TRACE | 轮次 | timeout |
|------|-------|------|---------|
| A | 关 | ×12 | 240s |
| B | `F203_L18_TRACE=1` | ×12 | 240s |

`ASCEND_DEVICE_ID=0` `CANNLAB=1` `Ascend910B3`；首轮 FORCE。

## 相对 N18 判读

| 干净 A | 干净 B | 推论 |
|--------|--------|------|
| 接近满绿、晚轮才挂 | B 挂/FAIL 明显更高 | 确认 TRACE-DC 抬失败（非仅污染） |
| A 仍早挂 | — | 粘性与污染弱相关，根因硬 |
| A/B 挂率接近 | B 挂时 16/16 | 探针主要观测；真挂独立 |

交付：`graph_tests/_outbox/FEEDBACK-NPU-AB-CLEAN.md`  
跑完提醒控制台关机（除非用户另有指示）。
