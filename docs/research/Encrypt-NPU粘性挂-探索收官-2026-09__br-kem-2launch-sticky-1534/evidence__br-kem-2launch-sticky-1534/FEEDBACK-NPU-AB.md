# FEEDBACK-NPU-AB — 关 TRACE vs TRACE-DC（同污染卡）

**权威来源**：[NPU-A/B Encaps 对照](bc-7b321f32-fd86-5c24-bff8-e5921d0465dc)  
**TS**：20260908_033144 · 分支 `cursor/kem-2launch-sticky-1534` @ `6db3f62`  
**卡**：未关机 → **同污染卡对照**（非干净卡）  
**日志**：`/opt/cursor/artifacts/npu_a_notrace_20260908_033144.log` · `npu_b_tracedc_*.log` · `npu_ab_summary_*.txt`

> 父侧另有 nohup job `npu_ab_20260908_040002` 与本刀重叠，已 **KILLED_DUP**；以下表以 subagent 交付为准。

## 汇总

| 刀 | TRACE | PASS | HANG | FAIL |
|----|-------|------|------|------|
| **NPU-A** | 关（unset） | **8** | **4** | 0 |
| **NPU-B** | `F203_L18_TRACE=1`（DataCopy） | **0** | **3** | **9** |

## NPU-A 细节

- HANG：r1 / r7 / r8 / r9 @ `launch 2 f203_encrypt_l18_l19` 无回  
- PASS：r2–r6、r10–r12  
- A-12 曾因节点离线中断，经 `cannlab-npu-1` 续跑完成

## NPU-B 细节

- FAIL r1–r9：l18 有回，`c.bin max≈224–247`  
- HANG r10–r12：`[l18-trace] stages set=16/16 : 0…15`

## 判读

1. **关 TRACE 仍可多轮绿（8/12）**，但同污染卡仍有 4 次 l18 Sync 挂 → 粘性与 TRACE 探针无关也能发生。  
2. **A 后立刻开 TRACE-DC → 0/12 绿**（FAIL 为主 + 末段 HANG 且 16/16）→ 支持 **TRACE-DC 在暖机/污染卡上显著抬失败与挂率**。  
3. 挂时 16/16 再证 **空 TRACE≠Prefix 前死**；真挂窗仍偏 **全 Mark 后 / Sync**。  
4. **须控制台关机后干净卡** 复验 A/B，否则污染主导无法定量。

## 下一步（父）

- 知识库 **N18**；图谱 `D-npu-ab-trace-contrast` → verified；`J-hang-after-full-trace` 加强  
- TRACE-DC 默认宜 **opt-in / 诊断专用**，勿作生产默开  
- 干净卡复验前：**请控制台关机**
