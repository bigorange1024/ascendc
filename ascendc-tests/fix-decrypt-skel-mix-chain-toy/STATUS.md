# STATUS — fix-decrypt-skel-mix-chain-toy

> Decrypt fused **握手**骨架 toy（TASK-009 建树；TASK-010 加 `SKEL_OMIT_SLOT0`）。不对 ML-KEM 正确性；SIM-only 门禁。

## 目标

| 档 | 配置 | 预期 |
|----|------|------|
| **A** | 默认 `SKEL_OMIT_SLOT0=0` `SKEL_OMIT_SET4=0` | SoftSync + 两轮 GATE 4/8 + stub NTT/INTT Cube 跑完；magic `SKELDEC1` / `out[8]=0x04` |
| **B（TASK-009）** | `SKEL_OMIT_SET4=1`，预算 60s | 双 AIV 两轮都不 SET(4) → AIC Wait(4) → **rc=124**（已证） |
| **C（TASK-010）** | `SKEL_OMIT_SLOT0=1` `SKEL_OMIT_SET4=0`，预算 60s | AIV0 不写 `s[0]`；AIV1 仍 `while(s[0]==0)`；SoftSync 为 SET(4) 前置 → 预期 **124** |

`SKEL_OMIT_SLOT0=1` 与 `SKEL_OMIT_SET4=1` **互斥**（`run.sh` 同时为 1 报错）。

## 握手（与生产同构的同步序）

```text
AIV0 stub_prep → SoftSyncArrive(slot0)；AIV1 自旋   # OMIT_SLOT0：AIV0 空操作不写哨兵
双 AIV SET(4)→WAIT(8)→Clear(slot0)                 # OMIT_SET4 跳过 SET(4)；OMIT_SLOT0 时 AIV0 仍可 SET(4)
NTT-like flag 1/3 + 轻量 MMAD
AIV0 stub_dot → SoftSyncArrive(slot1)；AIV1 自旋
双 AIV SET(4)→WAIT(8)→Clear(slot1)
INTT-like flag 1/3（无 flag 2）
AIV0 写 magic
AIC: WAIT(4)→SET(8) → Cube → WAIT(4)→SET(8) → Cube
```

## 验收记录

### TASK-009（2026-09-03）

| 档 | 命令 | rc | wall_sec | 备注 |
|----|------|-----|----------|------|
| A | `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` | **0** | **3.934** | magic `SKELDEC1` |
| B | `KERNEL_COMPUTE_BUDGET_SEC=60 SKEL_OMIT_SET4=1 …` | **124** | **60.393** | 支持 `J-omit-set4-hangs-decrypt` |

### TASK-010（2026-09-03 Cloud）

| 档 | 命令 | rc | wall_sec | 备注 |
|----|------|-----|----------|------|
| A | `SKEL_OMIT_SLOT0=0 SKEL_OMIT_SET4=0 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` | **0** | **3.223** | magic `SKELDEC1` / `0x04`；用例根无 stray dump |
| C | `KERNEL_COMPUTE_BUDGET_SEC=60 SKEL_OMIT_SLOT0=1 SKEL_OMIT_SET4=0 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` | **0** | **3.632** | **未挂**；`-DSKEL_OMIT_SLOT0=1` 已进 device flags；weaken `J-omit-slot0-spin-hangs`（禁止改参硬凑） |

日志：`/opt/cursor/artifacts/decrypt-skel-toy-slot0-A.log` · `decrypt-skel-toy-slot0-B.log`

## 禁止

- 真 unpack / su_dot / NTT Stage1–3；AIC Wait 中 SyncAll；双向 SoftSync；改 `examples/stable/**`
- `SKEL_OMIT_SLOT0` 与 `SKEL_OMIT_SET4` 同时为 1
