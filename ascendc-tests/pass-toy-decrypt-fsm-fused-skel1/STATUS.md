# STATUS — pass-toy-decrypt-fsm-fused-skel1

**任务**：DGT-20260903-3 / DGT-20260903-4；图谱 `Q-TOY-FUSED-SKEL` / `Q-TOY-MULTI-LAUNCH`  
**目的**：轻量 MIX：同构 Decrypt **SoftSync×2 + GATE 4↔8×2 + NTT/INTT flag 1/3** 融合握手骨架 + GT-4 TRACE；**不对 Alg.15 正确性**；只认 SIM。  
DGT-4：同进程 `TOY_LAUNCH_REPEAT`（默认 16）加压，搜 SynchronizeStream 挂死。

| 项 | 值 |
|----|-----|
| 拓扑 | `KERNEL_TYPE_MIX_AIC_1_2`，`blockDim=1`，全骨架单段 launch |
| SoftSync | slot0=prep、slot1=su；Arrive：AIV0 写 1 / AIV1 忙等；Clear：仅 AIV0 写 0 |
| GATE | 通道 `<2, PIPE_MTE2>`；双 AIV SET(4)→AIC WAIT(4)/SET(8)→双 AIV WAIT(8)×2 |
| NTT/INTT | 双 AIV SET(1)→AIC WAIT(1)/轻 MMAD/SET(3)→双 AIV WAIT(3)×2；**禁 Wait(2)**；**禁 flag 5/7** |
| AIC | GATE 无 MMAD；NTT/INTT 各一路 16×32×32；**禁 SyncAll**（含 Wait 期间） |
| TRACE | 8×32B 槽；AIV0 VECOUT DataCopy；AIC ones→A1→槽（GT-4）；Host 打印已置位槽 |
| Host | softSyncGm≥64B；`TOY_LAUNCH_REPEAT` 默认 16；每轮 H2D 清 TRACE+softSync → launch → Sync → 打印 TRACE |
| 禁令遵守 | 未改图谱；未 push；未抄 frozen；未改 stable；未改 SoftSync/GATE/NTT 握手语义 |
| CPU | 未作验收（`D-RG-SIM-PRIMARY`） |
| **SIM（单 launch，DGT-3）** | **PASS**（2026-09-03） |
| **SIM（×16 launch，DGT-4）** | **PASS**（2026-09-03） |

## SIM 证据（DGT-20260903-3，单 launch）

```bash
cd ascendc-tests/pass-toy-decrypt-fsm-fused-skel1
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

- 退出码 **0**；`[host] synchronize done (fused skel returned)`
- TRACE：`stages set=7/8`：`s0..s3=1`（AIV Soft0/NTT/Soft1/INTT），`s4=0`（AIC prep WAIT4 槽未落），`s5=1`，`s6=0x01010101`，`s7=1`
- 完成标记：`mark0=0x00F05E00`；`softSyncGm slot0=0 slot1=0`（Clear 后）
- `wall_sec≈1.836`，`KERNEL_COMPUTE_BUDGET_SEC=300`；`Total tick: 13056`
- 用例根无 stray `core*.dump` / `profile_*`
- 日志副本：`/opt/cursor/artifacts/DGT-20260903-3-sim.log`

## SIM 证据（DGT-20260903-4，同进程 ×16）

```bash
cd ascendc-tests/pass-toy-decrypt-fsm-fused-skel1
TOY_LAUNCH_REPEAT=16 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

- 退出码 **0**；`iter=0..15` 共 **16** 轮均 `FULL fused sync done`
- TRACE：iter0=`7/8`（s4=0）；iter1–15=`8/8`（含 s4）；末轮 `mark0=0x00F05E00`；`softSyncGm slot0=0 slot1=0`
- `wall_sec≈22.847`，`KERNEL_COMPUTE_BUDGET_SEC=600`；`Total tick: 181191`
- 用例根无 stray `core*.dump` / `profile_*`
- 日志副本：`/opt/cursor/artifacts/DGT-20260903-4-sim.log`
- SIM 尝试次数：**1**（未超 `MAX_SIM_ATTEMPTS=2`）

## 对 Q-TOY-FUSED-SKEL 的回答（DGT-3）

SIM 上 **Decrypt 同构双 SoftSync + 双 GATE + NTT/INTT 1/3 同核融合握手能跑完**，未出现 SynchronizeStream 挂死。  
`J-TWO-GATE-DIFF` / 第二 SoftSync 在本骨架下未触发挂点；AIC TRACE 首槽（prep WAIT4）偶发未落属 TRACE 可见性，非握手挂死。

## 对 Q-TOY-MULTI-LAUNCH 的回答（DGT-4）

同进程 **16** 轮「清 TRACE+softSync → launch 全融合骨架 → SynchronizeStream」均返回，**未挂死**。  
本骨架下 `Q-TOY-MULTI-LAUNCH` 暂不成立挂点；未改握手语义。
