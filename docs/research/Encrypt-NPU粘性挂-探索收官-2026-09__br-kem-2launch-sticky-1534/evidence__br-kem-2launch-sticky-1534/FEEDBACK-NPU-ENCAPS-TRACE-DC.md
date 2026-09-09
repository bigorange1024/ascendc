# FEEDBACK-NPU-ENCAPS-TRACE-DC

| 字段 | 值 |
|------|----|
| task_id | NPU-ENCAPS-TRACE-DC |
| verdict | **有条件完成**（DataCopy 消除挂时 0/16 空槽；12 轮无全绿；c.bin 对拍与 Host Sync 挂并存） |
| wall_clock_min | **~50**（含 FORCE 重建 + 12 轮×240s 上限） |
| directory | `examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-encaps-k4/` |
| remote | `developer@cannlab-npu:2222` · `ASCEND_DEVICE_ID=0` · `CANNLAB=1` · `Ascend910B3` |
| git | 工作区改 `compute/f203_encrypt_l18_l19_kernel.cpp`；**未** commit/push |
| rounds_requested | **12** |
| rounds_completed | **12**（无中断脚本） |

## 代码变更摘要

`FusedTraceMark`（仅 `stable-fips203-mlkem-kem-encaps-k4/compute/f203_encrypt_l18_l19_kernel.cpp`）：

| 路径 | 行为 |
|------|------|
| **默认** | AIV0：`FusedTraceAivAcc`（AIV 分支栈上 64B TBuf，Prefix 后 Init）→ 各 Mark 在 UB 累加槽位 → 整表 `DataCopy` 写回 GM；AIC 仍标量 |
| **对照** | `-DF203_L18_TRACE_SCALAR=1` 回退原标量 `trace[i]=1` |
| **工程注记** | toy E19b 每 Mark 新建 TPipe 可接受；Encaps 内须 **单次 Init + 复用 TBuf**，否则 UB 争用致 c.bin 红或 Host Sync 挂 |

未改 CrossCore FSM / GATE / 业务逻辑。

## REPORT 摘要（12 轮，`F203_L18_TRACE=1`，`TIMEOUT_SEC=240`）

| 轮次 | REPORT | `[l18-trace]` 摘要 |
|------|--------|-------------------|
| r1 | **HANG** | `stages set=16/16 : 0…15` |
| r2 | **FAIL** rc=1 | 无（kernel 已回；verify c 红） |
| r3 | **FAIL** rc=1 | 无 |
| r4 | **HANG** | `16/16 : 0…15` |
| r5 | **HANG** | `16/16 : 0…15` |
| r6 | **HANG** | `16/16 : 0…15` |
| r7 | **HANG** | `16/16 : 0…15` |
| r8 | **HANG** | `16/16 : 0…15` |
| r9 | **FAIL** rc=1 | 无 |
| r10 | **FAIL** rc=1 | 无 |
| r11 | **FAIL** rc=1 | 无 |
| r12 | **HANG** | `16/16 : 0…15` |

- **PASS**：**0 / 12**
- **HANG**（timeout 124）：**7** 轮（r1,r4–r8,r12）；挂时均见 **非空** TRACE（**16/16 全槽**）
- **FAIL**（verify / rc=1）：**5** 轮；`npu_launch` l18 **有回**，无 `[l18-trace]` 行（核时≪poll 500ms，正常）
- 对比标量 NPU-3：原 **PASS×10 → r11 HANG + `0/16` 空槽**；本刀 **无 PASS**，但 **凡 HANG 均 16/16 非空**

## 判读（对照 TASK 表）

| 结果模式 | 本跑次 | 图谱 effect |
|----------|--------|-------------|
| 12 轮绿且挂/成功可见非空 TRACE | ✗（0 绿） | — |
| 仍 r11 挂但 TRACE **非空** | **✓**（r12 HANG + `16/16`；r11 为 FAIL 非挂） | **support**：空槽为标量观测管道因子；真挂窗在 **全阶段 Mark 之后**（非 Prefix 前） |
| 仍 r11 挂且 **仍 0/16** | ✗ | H-E3 空槽路径 **weaken**（本刀挂时不见 0/16） |

**结论一句话**：Encaps `FusedTraceMark` 改为 AIV0 整表 DataCopy RMW 后，**粘性挂时 `[l18-trace]` 由 0/16 变为 16/16**，说明原空 TRACE 含观测写回因子；但本卡 12 轮 **0 PASS**（HANG/FAIL 交替），粘性真挂与 c.bin 对拍问题 **未** 因 Trace 修法 alone 消除。

## 日志 / 产物路径

| 跑次 | 路径 |
|------|------|
| 12 轮摘要 | `/opt/cursor/artifacts/npu_encaps_tracedc_12rounds_20260908_025122.log` |
| FORCE 重建 + r0 | `/opt/cursor/artifacts/npu_encaps_tracedc_20260908_024634.log` |
| 远程 | `/mnt/workspace/npu_encaps_tracedc_20260908_024634/` |

## 复现命令

```bash
# 本机改码后 scp 同路径至 cannlab-npu /mnt/workspace/ascendc/...
cd examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-encaps-k4
KEM_ENCAPS_FORCE_REBUILD=1 F203_L18_TRACE=1 bash run.sh -r npu -v Ascend910B3
```

## 范围合规

- 仅改 `stable-fips203-mlkem-kem-encaps-k4` 下 `f203_encrypt_l18_l19_kernel.cpp`；远程 scp 同步。
- **未** commit/push；**未**并行第二路 NPU。

---

> **请用户在 GitCode CANNLab 控制台关机**，避免 910B3 空转计费。
