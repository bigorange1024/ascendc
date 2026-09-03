# STATUS — pass-toy-encrypt-fsm-l18-skel1

**任务**：GT-20260903-7 / 图谱 `Q-TOY-SIM-2LAUNCH-HANG`（承接 GT-5/6 同核多 launch 未挂 → 本刀逼近 Encrypt 真 2-launch）  
**目的**：`TOY_SPLIT_2LAUNCH=1` 时每轮两段 ACLRT_LAUNCH（NTT_ONLY→Sync→GATE_INTT_ONLY→Sync），同进程 16 组 × 外层 2 独立进程，观察 `aclrtSynchronizeStream` 是否挂死；不对算法正确性。默认未设 env 时仍单 launch 全链路（GT-5/6 兼容）。

| 项 | 值 |
|----|-----|
| 拓扑 | `KERNEL_TYPE_MIX_AIC_1_2`，`blockDim=1`；可选 **两段 Host launch** |
| Host | `TOY_SPLIT_2LAUNCH=1`：每轮清 TRACE 一次 → launch0(`phase=NTT_ONLY`)→Sync→TRACE → launch1(`phase=GATE_INTT_ONLY`)→Sync→TRACE；`TOY_LAUNCH_REPEAT`=组数（默认 8，本刀 16） |
| 设备 | `tiling.phase`∈{FULL=0, NTT_ONLY=1, GATE_INTT_ONLY=2}；NTT_ONLY 跑完 NTT return；GATE_INTT_ONLY 跳过 NTT 直进 GATE→INTT |
| CrossCore | `<2, PIPE_MTE2>`；NTT/INTT **1/3**；GATE **4/8**（未改字面量） |
| TRACE | DataCopy 写槽（GT-4）；**每轮开始清一次**（两段累计槽） |
| 禁令遵守 | 无 SyncAll；无 SoftSync；无 INTT 5/7；未改图谱；未 push；未抄 frozen；未改 stable Encrypt |
| CPU | 未作验收（`D-RG-SIM-PRIMARY`） |
| **SIM** | **PASS**（2026-09-03，GT-7；2/2 进程） |

## 加压（GT-20260903-7）

```bash
cd ascendc-tests/pass-toy-encrypt-fsm-l18-skel1
TOY_SPLIT_2LAUNCH=1 TOY_LAUNCH_REPEAT=16 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
# 串行重复 2 次独立进程（禁并行）
```

| 程 | exit | 两段 sync+TRACE | kernel wall_sec | Total tick | 整树 wall≈ |
|----|------|-----------------|-----------------|------------|------------|
| 1/2 | 0 | **16/16 组**（NTT_ONLY×16 + GATE_INTT×16） | 64.032 | 333830 | ~71s |
| 2/2 | 0 | **16/16 组** | 58.573 | 333645 | ~65s |

- budget 默认 **240s**；无 TIMEOUT(124)、无 HANG
- 末轮 TRACE：`NTT_ONLY` 常见 `set=3/8 : 0 1 4`；`GATE_INTT_ONLY` 累计 `set=7/8 : 0 1 2 3 4 5 6`
- 用例根无 stray `core*.dump` / `profile_*`
- 日志：`/opt/cursor/artifacts/GT-20260903-7-sim.log`
- SIM 尝试次数：2 / 2（均 PASS）

## 相对 GT-6 / 图谱结论要点

| 问题 | 本刀观察 |
|------|----------|
| `Q-TOY-SIM-2LAUNCH-HANG`：两段 Host launch 是否 SynchronizeStream 挂死 | **未挂**：2 进程×16 组两段均 sync done + TRACE |
| 与 `F-SIM-LAUNCH` | toy 尺度上逼近生产 Encrypt 2-launch；**未**据此推荐「堆 Host launch 当修复」 |

## 与 Encrypt 同构意图

| Encrypt l18 | 本玩具 |
|-------------|--------|
| 生产侧 2 Host launch（prep/NTT → GATE+INTT） | `TOY_SPLIT_2LAUNCH=1` + `phase` |
| 同核 NTT 1/3 → GATE 4↔8 → INTT 1/3 | 拆成两段；flag 字面量未改 |
| FusedTraceMark | DataCopy TRACE + 每段 Sync 后打印 |
