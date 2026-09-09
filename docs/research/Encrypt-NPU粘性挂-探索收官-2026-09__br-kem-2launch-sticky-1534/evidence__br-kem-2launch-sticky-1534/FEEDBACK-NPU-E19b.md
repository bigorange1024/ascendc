# FEEDBACK-NPU-E19b

| 字段 | 值 |
|------|----|
| task_id | NPU-E19b |
| verdict | **PASS（SIM×8 + NPU×12 全绿）** |
| wall_clock_min | **~3**（NPU kernel 2.5s；含编译总 ~25s；deadline 40） |
| directory | `graph_tests/toys/toy-e19-early-entry-trace/` |
| remote | `developer@cannlab-npu:2222` · `ASCEND_DEVICE_ID=0` · `CANNLAB=1` · `Ascend910B3` |
| git | `6db3f62` @ `cursor/kem-2launch-sticky-1534`（本地改 mmad_custom.cpp；**未** commit/push） |
| rounds_requested | SIM **8** / NPU **12** |
| rounds_completed | **8 / 12 全绿** |
| kernel_wall_sec | SIM **66.8s** / NPU **2.454s**（budget 600；rc=**0**） |

## 代码变更摘要

`TraceSlotStore`（`mmad_custom.cpp`）：

| 路径 | 行为 |
|------|------|
| **默认** | AIV：`int32[16]` 整表 GM→UB→改单槽→UB→GM（64B DataCopy RMW）；AIC：保留标量（SIM 不依赖 AIC D2H） |
| **对照** | `-DTOY_E19_TRACE_SCALAR=1` 回退 Encaps 同形标量直写 |

**迭代**：首轮实现「单元素 DataCopy」SIM 红（pop=0）；「8 元素块写」SIM 绿但 NPU r0 仍丢槽 15；**整表 RMW** 后 SIM×8 + NPU×12 均绿。

## REPORT 摘要

| 线索 | 观测 |
|------|------|
| `[e19-trace]` | NPU r0–r11 均为 `stages set=2/16 : 0 15` |
| Host `100` / `111` | 各 **12** 次 |
| `SUCCESS` / `REPORT: E19 PASS` | **有** |
| `TIMEOUT` | **无** |
| 对比 E19 标量 | E19 r3 丢槽 15；E19b **12 轮无退化** |

### 入口槽可见性

| 轮次 | 槽 **0** | 槽 **15** | Host 111 |
|------|----------|-----------|----------|
| 0–11 | ✓ | ✓ | ✓ |

## 对图谱节点的 effect

对照 `TASK-E19b` 判读表：

| 结果模式 | 本跑次 | 图谱 effect |
|----------|--------|-------------|
| 12 轮 0+15 全绿 | **✓** | **support**：空 TRACE / 入口标观测管道含 **AIV 标量→D2H** 因子；Encaps `FusedTraceMark` 同形标量须警惕 |
| 仍第 N 轮丢 15 | ✗ | — |
| TIMEOUT 粘性挂 | ✗ | — |

| node | effect | note |
|------|--------|------|
| `D-exp-e19b` | **support** | AIV DataCopy RMW 消除 NPU 多轮槽 15 丢失 |
| **H-E4**（非 DataCopy 问题） | **weaken** | 根因含标量写回 + 对齐/块粒度；非「换 API 也无效」 |
| **H-E3**（入口标全无） | **weaken** | 加固后 12 轮稳 |
| Encaps 粘性挂 | **cite** | 若 Encaps 仍用标量 FusedTraceMark，应评估同形 DataCopy RMW |
| `J-sim-not-sticky` | **neutral** | SIM 仍绿；NPU 从「r3 退化」→「12 轮稳」 |

## 日志 / 产物路径

| 跑次 | 路径 |
|------|------|
| SIM×8 | `/opt/cursor/artifacts/e19b-sim8.log` |
| 远程 NPU 主日志 | `/mnt/workspace/npu_e19b_20260908_021745/run.log` |
| scp 副本 | `/opt/cursor/artifacts/npu_e19b_20260908_021745.log` |
| host_trace | `/opt/cursor/artifacts/npu_e19b_host_trace.log` |
| 远程失败试探 | `/mnt/workspace/npu_e19b_20260908_021516/run.log`（块写版 r0 丢 15） |

## 复现命令

**SIM（本地）**

```bash
cd graph_tests/toys/toy-e19-early-entry-trace
TOY_ROUNDS=8 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

**NPU（远程）**

```bash
source /home/developer/Ascend/ascend-toolkit/set_env.sh
cd /mnt/workspace/ascendc/graph_tests/toys/toy-e19-early-entry-trace
export ASCEND_DEVICE_ID=0 CANNLAB=1 TOY_ROUNDS=12 KERNEL_COMPUTE_BUDGET_SEC=600
bash run.sh -r npu -v Ascend910B3
```

## 范围合规

- 仅改 `toy-e19-early-entry-trace/mmad_custom.cpp`；远程 scp 同步。
- **未**改 Encaps/stable/图谱 yaml；**未** commit/push；**未**并行第二路 NPU。

---

> **请用户在 GitCode CANNLab 控制台关机**，避免 910B3 空转计费。
