# STATUS — fix-decrypt-skel-mix-chain-toy

> Decrypt fused **握手**骨架 toy（TASK-009 建树；TASK-010 `SKEL_OMIT_SLOT0`；TASK-011 `SKEL_OMIT_SET4_R2`；TASK-012 `SKEL_SOFTSYNC_PREFILL`）。不对 ML-KEM 正确性；SIM-only 门禁。

## 目标

| 档 | 配置 | 预期 |
|----|------|------|
| **A** | 默认 `SKEL_OMIT_SET4_R2=0` `SKEL_OMIT_SET4=0` `SKEL_OMIT_SLOT0=0` `SKEL_SOFTSYNC_PREFILL=0` | SoftSync + 两轮 GATE 4/8 + stub NTT/INTT Cube 跑完；magic `SKELDEC1` / `out[8]=0x04` |
| **B（TASK-009）** | `SKEL_OMIT_SET4=1`，预算 60s | 双 AIV 两轮都不 SET(4) → AIC Wait(4) → **rc=124**（已证） |
| **C（TASK-010）** | `SKEL_OMIT_SLOT0=1`，预算 60s | AIV0 不写 `s[0]`；AIV1 空 while → SIM **未挂**（rc=0）→ SoftSync 空自旋非 SIM hang 代理 |
| **D（TASK-011）** | `SKEL_OMIT_SET4_R2=1`，预算 60s | 第一轮 GATE 仍 SET(4)；第二轮 slot1 **不** SET(4) → AIC 卡第二段 Wait(4) → **rc=124** |
| **E（TASK-012）** | `SKEL_SOFTSYNC_PREFILL=0` / `=1`（Host 运行时；OMIT_*=0） | 清零与脏预填 **均绿**（脏哨兵≠SIM hang；预填 1≈误放行） |

`SKEL_OMIT_SET4` / `SKEL_OMIT_SLOT0` / `SKEL_OMIT_SET4_R2` **三者互斥**（`run.sh` 同时开多个报错）。`SKEL_SOFTSYNC_PREFILL` 为 Host 运行时 env，**非** cmake 宏、**非** hang 注入。

## 握手（与生产同构的同步序）

```text
AIV0 stub_prep → SoftSyncArrive(slot0)；AIV1 自旋
双 AIV SET(4)→WAIT(8)→Clear(slot0)          # OMIT_SET4 跳过；OMIT_SET4_R2 仍 SET
NTT-like flag 1/3 + 轻量 MMAD
AIV0 stub_dot → SoftSyncArrive(slot1)；AIV1 自旋
双 AIV SET(4)→WAIT(8)→Clear(slot1)          # OMIT_SET4_R2：省略 SET(4)
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
| A | `SKEL_OMIT_SLOT0=0 SKEL_OMIT_SET4=0 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` | **0** | **3.223** | magic `SKELDEC1` / `0x04` |
| C | `KERNEL_COMPUTE_BUDGET_SEC=60 SKEL_OMIT_SLOT0=1 …` | **0** | **3.632** | **未挂**；weaken `J-omit-slot0-spin-hangs` |

日志：`/opt/cursor/artifacts/decrypt-skel-toy-slot0-A.log` · `decrypt-skel-toy-slot0-B.log`

### TASK-011（2026-09-03 Cloud）

| 档 | 命令 | rc | wall_sec | 备注 |
|----|------|-----|----------|------|
| A | `SKEL_OMIT_SET4_R2=0 SKEL_OMIT_SET4=0 SKEL_OMIT_SLOT0=0 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` | **0** | **3.439** | magic `SKELDEC1` / `0x04`；用例根无 stray dump |
| D | `KERNEL_COMPUTE_BUDGET_SEC=60 SKEL_OMIT_SET4_R2=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` | **124** | **60.103** | device flags `-DSKEL_OMIT_SET4_R2=1`；**support** `J-omit-slot1-hangs-after-ntt` |

日志：`/opt/cursor/artifacts/decrypt-skel-toy-r2-A.log` · `decrypt-skel-toy-r2-B.log`

### TASK-012（2026-09-03 Cloud）

| 档 | 命令 | rc | wall_sec | 备注 |
|----|------|-----|----------|------|
| E0 | `SKEL_SOFTSYNC_PREFILL=0 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` | **0** | **3.860** | Host 清零；magic `SKELDEC1` / `0x04`；OMIT_*=0 |
| E1 | `SKEL_SOFTSYNC_PREFILL=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` | **0** | **4.078** | Host 脏写 int32[2]=1；**仍绿** → **support** `J-dirty-softsync-hang-vs-race`（脏≠hang） |

日志：`/opt/cursor/artifacts/decrypt-skel-toy-dirty0.log` · `decrypt-skel-toy-dirty1.log`  
未改设备 SoftSync；未开任何 OMIT_*；用例根无 stray dump。

## 禁止

- 真 unpack / su_dot / NTT Stage1–3；AIC Wait 中 SyncAll；双向 SoftSync；改 `examples/stable/**`
- `SKEL_OMIT_SET4` / `SKEL_OMIT_SLOT0` / `SKEL_OMIT_SET4_R2` 同时开多个
- 用空 while / volatile 硬凑 SoftSync hang
- 把 `SKEL_SOFTSYNC_PREFILL=1` 当成 hang 注入（本开关预期仍绿）
