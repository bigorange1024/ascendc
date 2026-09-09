# TASK-NPU-ENCAPS-TRACE-DC — Encaps FusedTraceMark DataCopy + 多轮

**deadline_min**: 50  
**图谱**：接 NPU-E19/E19b — 标量 fused-trace 多轮 D2H 丢槽；整表 DataCopy RMW 修好 E19。

## 目标

1. **改码**（本机）：`examples/stable/.../kem-encaps-k4/compute/f203_encrypt_l18_l19_kernel.cpp` 的 `FusedTraceMark`  
   - 当 `traceGm != nullptr` 时用与 E19b **同构**的整表 int32[16] RMW（GM→UB 改一槽→DataCopy 回），**禁止**再纯标量 `trace[i]=1`（可 `#if` 保留对照）。  
   - 详细中文注释：背景=E19b NPU 发现；结论=观测/可能粘性相关。  
   - **仅改 Trace 写回**；禁止改 CrossCore FSM / GATE / 业务。

2. 可选同形：若 `pke-encrypt-k4` 共用同文件副本，**只改 encaps-k4 路径**（白名单）。

3. **同步到远程** cannlab-npu（scp/tar 改动的 cpp；勿 git push）。

4. **NPU 跑**：干净优先；`F203_L18_TRACE=1` Encaps `TOY` 式多轮 **12**；`TIMEOUT_SEC=240` 每轮；FORCE 首轮重建。  
   - 日志 `/mnt/workspace/npu_encaps_tracedc_*` + scp artifacts  
   - 盯：`REPORT` PASS/HANG、`[l18-trace]` 是否仍 0/16、挂轮次

## 判读

| 反馈 | 计划 |
|------|------|
| 12 轮绿且挂时/成功可见非空 TRACE | **重大**：标量 Mark 是观测假象或粘性因子 → 固化 DataCopy；削弱「Prefix 前死」若 TRACE 显示已过 15 |
| 仍 r11 挂但 TRACE **非空** | 真挂窗钉到具体槽；H-E1 可重标 |
| 仍 r11 挂且 **仍 0/16** | DataCopy 不足以解释空槽/挂；回 H-E3/调度 |

## FEEDBACK

`graph_tests/_outbox/FEEDBACK-NPU-ENCAPS-TRACE-DC.md`  
提醒关机。禁并行 NPU；禁改 FSM；禁 commit/push。
