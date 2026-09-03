# STATUS — fix-toy-encrypt-fsm-ntt1

**任务**：GT-20260903-1 / 图谱 `Q-TOY-NTT`  
**目的**：轻量 MIX 单段 CrossCore flag **1/3**（同构 Encrypt NTT 握手）；**不对算法正确性**；只认 SIM 跑完。

| 项 | 值 |
|----|-----|
| 拓扑 | `KERNEL_TYPE_MIX_AIC_1_2`，`blockDim=1` |
| CrossCore | `<2, PIPE_MTE2>`；AIV SET(1) → AIC WAIT(1)+MMAD 16×32×32 → AIC SET(3) → AIV WAIT(3) |
| 桩哈希 | AIV `SetValue` 填常数写 S0 半片（禁真 SHAKE；`Duplicate` 不支持 int8） |
| 禁令遵守 | 无 SyncAll；无 SoftSync；无 GATE/INTT；未改图谱；未 push |
| CPU | 未作验收（`D-RG-SIM-PRIMARY`） |
| **SIM** | **PASS**（2026-09-03） |

## SIM 证据

```bash
cd ascendc-tests/fix-toy-encrypt-fsm-ntt1
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

- 退出码 **0**；`[host] synchronize done`；`[SUCCESS] … kernel exit=0`
- `wall_sec≈1.66`，`KERNEL_COMPUTE_BUDGET_SEC=180`；`Total tick: 8000`
- 用例根无 stray `core*.dump` / `profile_*`；dump 在 `OPPROF_*/dump/`（若有）
- 日志副本：`/opt/cursor/artifacts/GT-20260903-1-sim.log`

## 与 Encrypt 同构意图

| Encrypt NTT(y) | 本玩具 |
|----------------|--------|
| SET(1)/WAIT(1)/SET(3)/WAIT(3) | 同字面量 |
| 真 Stage1 + 四路 MMAD | 桩哈希半片 + 一路 16×32×32 |
| 真 Pack/merge | 仅写 32B 完成标记 |
