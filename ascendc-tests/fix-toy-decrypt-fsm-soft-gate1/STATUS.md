# STATUS — fix-toy-decrypt-fsm-soft-gate1

**任务**：DGT-20260903-2 / 图谱 `Q-TOY-SOFT-GATE`  
**目的**：轻量 MIX：生产同构 **SoftSyncArrive(slot0)** 之后再一轮 GATE **4↔8**（同构 Decrypt prep 段末）；**不对算法正确性**；只认 SIM 跑完且 Host 见完成标记。

| 项 | 值 |
|----|-----|
| 拓扑 | `KERNEL_TYPE_MIX_AIC_1_2`，`blockDim=1`，单 launch |
| SoftSync | slot0：AIV0 `s[0]=1`+`PipeBarrier`；AIV1 `while(s[0]==0)`+`PipeBarrier`（字面同构生产） |
| GATE | 通道 `<2, PIPE_MTE2>`；双 AIV SET(4) → AIC WAIT(4)→MMAD→SET(8) → 双 AIV WAIT(8) |
| AIC | WAIT(4) 后极轻 MMAD 16×32×32；**禁 SyncAll**（含 Wait 期间） |
| TRACE | SoftSync+GATE 后双 AIV GT-4 式 DataCopy（8×int32/槽）；禁 Duplicate(int8) |
| Host | softSyncGm≥64B H2D 清零；单 launch；`aclrtSynchronizeStream` |
| 禁令遵守 | 无 NTT/INTT；无第二轮 GATE；未用 CrossCore 代替 SoftSync；未改图谱；未 push；未抄 frozen；未改 stable |
| CPU | 未作验收（`D-RG-SIM-PRIMARY`） |
| **SIM** | **PASS**（2026-09-03） |

## SIM 证据

```bash
cd ascendc-tests/fix-toy-decrypt-fsm-soft-gate1
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

- 退出码 **0**；`[host] synchronize done (SoftSync+GATE returned)`
- TRACE：`s0=0x005F6800 s1=0x005F6801`（双 AIV 均过 SoftSync+WAIT(8)）；`softSyncGm slot0=1`
- `wall_sec≈1.035`，`KERNEL_COMPUTE_BUDGET_SEC=180`；`Total tick: 5073`
- 用例根无 stray `core*.dump` / `profile_*`
- 日志副本：`/opt/cursor/artifacts/DGT-20260903-2-sim.log`

## 对 Q-TOY-SOFT-GATE 的回答（本刀）

SIM 上 **SoftSyncArrive 后再一轮 GATE 4↔8 能跑完**，未出现 SynchronizeStream 挂死。  
本刀只测一轮 GATE（`J-TWO-GATE-DIFF` 仍未证）；若生产卡死，更可能需叠第二轮 GATE 或同核 NTT/INTT（后续 DGT）。
