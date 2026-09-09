# FEEDBACK-NPU-E19

| 字段 | 值 |
|------|----|
| task_id | NPU-E19 |
| verdict | **FAIL（中途；非 TIMEOUT）** |
| wall_clock_min | **~0.5**（kernel 12.7s；含编译总 ~32s；deadline 45） |
| directory | `graph_tests/toys/toy-e19-early-entry-trace/` |
| remote | `developer@cannlab-npu:2222` · `ASCEND_DEVICE_ID=0` · `CANNLAB=1` · `Ascend910B3` |
| git | `6db3f62` @ `cursor/kem-2launch-sticky-1534` |
| rounds_requested | **12**（`TOY_ROUNDS=12`） |
| rounds_completed | **3 全绿 + 第 4 轮（index=3）FAIL** |
| kernel_wall_sec | **12.710**（budget 600；rc=**21** 非 124） |

## REPORT 摘要

| 线索 | 观测 |
|------|------|
| `REPORT:` | **无**（本用例无 Encaps 式 REPORT 行） |
| `[e19-trace]` | r0–r2：`stages set=2/16 : 0 15`；**r3：`set=1/16 : 0`（缺 15）** |
| Host `100` | **4** 次（发起 4 轮 launch） |
| Host `111` | **3** 次（r3 未达 Sync 验收） |
| `SUCCESS` | **无** |
| `TIMEOUT` | **无**（`kernel-run-timeout` rc=21，墙钟 12.7s ≪ 600s） |
| 设备码（r3） | AIV0 `500/504/505/502` **仍打印**（504=已写槽15）；AIC `400–402` 在 Host FAIL 之后才续出 |
| 失败文案 | `[FAIL] entry slots 0/15 missing after round 3 (pop=1)` |

### 入口槽可见性（核心问题）

| 轮次 | 槽 **0** | 槽 **15** | Host 111 |
|------|----------|-----------|----------|
| 0 | ✓ | ✓ | ✓ |
| 1 | ✓ | ✓ | ✓ |
| 2 | ✓ | ✓ | ✓ |
| 3 | ✓ | **✗**（D2H 未见；设备侧仍报 504） | ✗ |

**结论**：实机 **前 3 轮入口槽 0/15 均可见**；第 4 轮起 **D2H 丢失槽 15**（槽 0 仍见）。非「从一开始入口标全无」，亦非 12 轮全绿。E19 stub 在 NPU 上呈现 **粘性退化**（≈3 轮后槽 15 从 Host 视角消失）。

## 对图谱节点的 effect

对照 `TASK-NPU-E19` 判读表：

| 结果模式 | 本跑次 | 图谱 effect |
|----------|--------|-------------|
| 12 轮绿且见 0/15 | ✗ | — |
| 中途挂且入口槽也无 | ✗（r0–r2 双槽可见；r3 仍有 0） | **不支持 H-E3** |
| 中途挂但入口槽有 | **✓** | **非 H-E3**；与 Encaps 粘性挂同族——入口协议曾活、多轮后退化；下一刀仍应对照 Encaps Mark-before-Prefix，而非归因「管道从未见过入口标」 |

| node | effect | note |
|------|--------|------|
| `D-exp-e19` | **partial support / NPU 粘性** | SIM×8 全绿；NPU 仅 3 轮稳、第 4 轮槽 15 D2H 丢 |
| **H-E3**（入口标全无） | **weaken** | 实机曾见 0/15×3；r3 为部分丢失而非全无 |
| **H-E1**（入口有、业务 Mark 无） | **cite / 预备** | 本刀为纯入口 stub，未验 Prefix 后业务标 |
| `J-sim-not-sticky` | **support（反向）** | SIM 8 轮不粘；**NPU 第 4 轮即退化** → SIM 绿 ≠ NPU 稳 |
| Encaps 粘性挂 | **support（同族）** | 多轮后 Host 观测恶化；非挂死（12.7s 即返） |

## 日志 / 产物路径

| 跑次 | 路径 |
|------|------|
| 远程主日志 | `/mnt/workspace/npu_e19_20260908_100928/run.log` |
| scp 副本 | `/opt/cursor/artifacts/npu_e19_20260908_100928.log` |
| host_trace | `/opt/cursor/artifacts/npu_e19_host_trace.log` |
| SIM 对照 | `/opt/cursor/artifacts/e19-default-sim.log`（见 `FEEDBACK-E19.md`） |

## 复现命令（远程）

```bash
source /home/developer/Ascend/ascend-toolkit/set_env.sh
cd /mnt/workspace/ascendc/graph_tests/toys/toy-e19-early-entry-trace
export ASCEND_DEVICE_ID=0 CANNLAB=1 TOY_ROUNDS=12 KERNEL_COMPUTE_BUDGET_SEC=600
bash run.sh -r npu -v Ascend910B3
```

## 范围合规

- 仅远程跑 E19 + 写 `/mnt/workspace/npu_e19_*`；本机仅 `_outbox/FEEDBACK-NPU-E19.md` + artifacts。
- **未**改 Encaps FSM / stable / 图谱 yaml；**未** commit/push；**未**并行第二路 NPU。

---

> **请用户在 GitCode CANNLab 控制台关机**，避免 910B3 空转计费。
