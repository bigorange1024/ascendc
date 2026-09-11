# STATUS — pass-toy-encrypt-fsm-gate-intt1

**任务**：GT-20260903-2 / 图谱 `Q-TOY-GATE-INTT`  
**目的**：轻量 MIX：GATE CrossCore **4↔8** 后再跑 INTT 同构 **flag 1/3**；**不对算法正确性**；只认 SIM 跑完。

| 项 | 值 |
|----|-----|
| 拓扑 | `KERNEL_TYPE_MIX_AIC_1_2`，`blockDim=1`，单 Host launch |
| 核内序 | 桩前缀 →（跳过短 NTT）→ GATE 4↔8 → INTT 1/3 + 极轻 MMAD → 完成标记 |
| CrossCore | `<2, PIPE_MTE2>`；GATE **4/8**；INTT 复用 **1/3**（禁 **5/7**） |
| 桩哈希 | AIV `SetValue` 填常数写 S0 半片（禁真 SHAKE；禁 `Duplicate(int8)`） |
| 禁令遵守 | 无 SyncAll；无 SoftSync；未改图谱；未 push；未抄 frozen |
| CPU | 未作验收（`D-RG-SIM-PRIMARY`） |
| **SIM** | **PASS**（2026-09-03） |

## SIM 证据

```bash
cd ascendc-tests/pass-toy-encrypt-fsm-gate-intt1
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

- 退出码 **0**；`[host] synchronize done`；`[SUCCESS] … kernel exit=0`
- `wall_sec≈1.837`，`KERNEL_COMPUTE_BUDGET_SEC=180`；`Total tick: 10911`
- 用例根无 stray `core*.dump` / `profile_*`；CaModel dump 在 `sim_log/`
- 日志副本：`/opt/cursor/artifacts/GT-20260903-2-sim.log`
- SIM 尝试次数：1 / 2

## 与 Encrypt 同构意图

| Encrypt l18 | 本玩具 |
|-------------|--------|
| GATE SET(4)/WAIT(4)/SET(8)/WAIT(8) | 同字面量 |
| INTT 复用 1/3 | 同字面量；禁 5/7 |
| 真 at_jp / SHAKE | 跳过 / SetValue 桩 |
| 可选短 NTT 1/3 | **跳过**（保证 INTT 独占 1/3 生命周期） |
