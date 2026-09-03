# STATUS — fix-toy-decrypt-fsm-softsync1

**任务**：DGT-20260903-1 / 图谱 `Q-TOY-SOFTSYNC`  
**目的**：轻量 MIX 仅测生产 Decrypt **SoftSyncArrive**（AIV0 对 GM slot0 标量写 1，AIV1 `while==0` 忙等）；**不对算法正确性**；只认 SIM 跑完且 Host 见完成标记。

| 项 | 值 |
|----|-----|
| 拓扑 | `KERNEL_TYPE_MIX_AIC_1_2`，`blockDim=1` |
| SoftSync | slot0：AIV0 `s[0]=1`+`PipeBarrier`；AIV1 `while(s[0]==0)`+`PipeBarrier`（字面同构生产） |
| AIC | **无 WaitFlag**；极轻 MMAD 16×32×32；禁 SyncAll |
| TRACE | SoftSync 后双 AIV GT-4 式 DataCopy（8×int32/槽）；禁 Duplicate(int8) |
| Host | softSyncGm≥64B H2D 清零；单 launch；`aclrtSynchronizeStream` |
| 禁令遵守 | 无 CrossCore 代替 SoftSync；无 SyncAll；无 GATE/NTT/INTT；未改图谱；未 push；未抄 frozen；未改 stable |
| CPU | 未作验收（`D-RG-SIM-PRIMARY`） |
| **SIM** | **PASS**（2026-09-03） |

## SIM 证据

```bash
cd ascendc-tests/fix-toy-decrypt-fsm-softsync1
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

- 退出码 **0**；`[host] synchronize done (SoftSyncArrive returned)`
- TRACE：`s0=0x0050F700 s1=0x0050F701`（双 AIV 均过 SoftSync）；`softSyncGm slot0=1`
- `wall_sec≈0.834`，`KERNEL_COMPUTE_BUDGET_SEC=180`；`Total tick: 3979`
- 用例根无 stray `core*.dump` / `profile_*`
- 日志副本：`/opt/cursor/artifacts/DGT-20260903-1-sim.log`

## 对 Q-TOY-SOFTSYNC 的回答（本刀）

SIM 上 **AIV-AIV SoftSyncArrive（标量写 + 忙等）能跑完**，未出现 SynchronizeStream 挂死。  
`J-PRIMARY-SOFTSYNC`（Decrypt 独有忙等最像永久挂）在本隔离 toy 上 **未被证成**；若生产卡死，更可能需结合 GATE/NTT/INTT 融合骨架或其它因素（后续 DGT-2/3）。
