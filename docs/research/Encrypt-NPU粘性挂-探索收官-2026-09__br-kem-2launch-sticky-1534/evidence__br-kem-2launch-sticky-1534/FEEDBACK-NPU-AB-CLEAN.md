# FEEDBACK-NPU-AB-CLEAN — 干净卡（bootstrap 后）· **中途按首挂政策停**

**job**：`/mnt/workspace/jobs/npu_ab_20260908_114644` · `cannlab-npu-1`=`100.85.76.12`  
**停因**：用户指出「已知会卡死仍空等满轮」→ 父 **STOPPED_BY_PARENT**（B r3 进行中杀掉）  
**脚本改正**：`run_npu_ab_nohup.sh` 默认 `STOP_ON_HANG=1 MAX_HANG=1`

## 已拿到的结果（够用）

### A · 关 TRACE（完整 12 轮）

| | |
|--|--|
| PASS | **10**（r1–r10） |
| HANG | **2**（r11、r12 @ `launch 2 l18`） |

→ 干净冷启复现 N10：**前 10 绿、约第 11 轮起粘性挂**。

### B · TRACE-DC（只跑到 r2，r3 被杀）

| | |
|--|--|
| r1 | **HANG** + `[l18-trace] 16/16` |
| r2 | **HANG** + `16/16` |

→ 暖机/刚挂过后开 TRACE-DC：**首轮即挂且满槽**（与 N18「抬挂」同向；样本已够，无需再烧 240s×10）。

## 对照 N18（污染卡）

| | 污染 N18 | 干净本刀 |
|--|----------|----------|
| 关 TRACE | 8 PASS / 4 HANG | **10 PASS / 2 HANG（r11+）** |
| TRACE-DC | 0 PASS / 多 HANG·FAIL | **首两轮即 HANG + 16/16** |

## 方法教训

猎挂/对照：**现象已出现就停**；240s 只用于确认「这一轮是挂」；满轮挂率统计须显式 `STOP_ON_HANG=0`，不得当默认。

## 请控制台关机
