# FEEDBACK-E21

| 字段 | 值 |
|------|----|
| task_id | E21 |
| verdict | **PASS**（静态+audit+encaps SIM×1） |
| wall_clock_min | **~4**（分析+audit+SIM；SIM wall≈107s） |
| scope | 只读 `f203_encrypt_l18_l19_kernel.cpp`；禁改 Encrypt 业务码；禁 NPU；禁 commit |
| hypothesis | 收紧「最后 CrossCore → Host Sync」窗：AIC 早退后 AIV-only 尾段 **零 CrossCore** |
| artifact | `/opt/cursor/artifacts/e21-l18-sync-audit.json` |

## 1. 末段时间线（AIC 最后 FsmSet/Wait 之后）

源文件：`examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-encaps-k4/compute/f203_encrypt_l18_l19_kernel.cpp`

### AIC 末段（`if (aic)`，L230–255）

| 行号 | 动作 | 状态 / flag |
|------|------|-------------|
| 249–250 | `FsmWait(ST_NTT_AIV_SPLIT)` | flag **1**（等 AIV INTT Split） |
| 251–253 | `AicMmadRound` INTT + `TR_AIC_INTT_MMAD` | — |
| **254–255** | **`FsmSet(ST_NTT_AIV_PACK)`** | flag **3** ← **AIC 最后一次 CrossCore** |
| （其后） | **无** Wait/Set/Mark；`if (aic)` 块结束即 return | — |

结论：AIC 在 INTT `FsmSet(PACK=3)` 后**立即结束**，不再参与 CrossCore。

### AIV 与之配对的最后 CrossCore（`else`，L402–408）

| 行号 | 动作 | 状态 / flag |
|------|------|-------------|
| 403–404 | `FsmSet(ST_NTT_AIV_SPLIT)` | flag **1**（放 AIC 进 INTT MMAD） |
| **407–408** | **`FsmWait(ST_NTT_AIV_PACK)`** | flag **3** ← **AIV 最后一次 CrossCore** |
| 409–420 | INTT Pack + RouteUV merge | 无 CrossCore |
| 421 | `FusedTraceMark(TR_AIV_INTT_U)` | 设备 Mark，非 CrossCore |
| 423–426 | `mod_q_add_gm_halfrows(u+=e1)` + `TR_AIV_E1_DONE` | AIV-only |
| 428–431 | subBlock0：`mod_q_add_gm_single_row(v+=e2)` + **`TR_AIV_V_DONE`** | AIV-only |
| **435** | **`tail_pack_shard_gm(uOut,vOut,cGm,subBlockID)`** | AIV-only；其后无任何 Fsm*/CrossCore |

### 末段是否零 CrossCore？

| 窗 | CrossCore？ | 说明 |
|----|-------------|------|
| AIC：`FsmSet(PACK)` 之后 → kernel 结束 | **零** | AIC 早退 |
| AIV：`FsmWait(PACK)` 之后 → `mod_q` / `V_DONE` / `tail_pack` → 结束 | **零** | 仅 PipeBarrier + Mark + GM 业务 |
| 全 Mark 之后 → Host Sync | **零 CrossCore** | 粘性挂窗候选恰落在此 AIV-only 尾段 |

**一句话结论**：AIC 最后 `FsmSet(ST_NTT_AIV_PACK=3)`（L255）后即退；AIV 在对应 `FsmWait(PACK)`（L408）之后仍做 INTT merge、`mod_q`、`TR_AIV_V_DONE`、`tail_pack_shard_gm`，**该窗零 CrossCore**。

## 2. sync_audit 摘要

| 项 | 值 |
|----|----|
| 命令 | `python3 thirdparty/cannbot-skills/ops/ascendc-sync-audit/scripts/sync_audit.py …/f203_encrypt_l18_l19_kernel.cpp --check all --format json` |
| 输出 | `/opt/cursor/artifacts/e21-l18-sync-audit.json` |
| exit | **0** |
| event_count | 16 |
| findings | **17**（SYNC-03×1 + SYNC-09×16） |
| SYNC-03 | **假阳性（对照 E17）**：包装 `FsmWait`/`FsmSet`（L140–155）内 `CrossCoreWaitFlag`/`SetFlag` 被静态判「同侧」；真实配对分落 `if (aic)` / `else` AIV 块 |
| SYNC-09 | 性能提示：`PipeBarrier<PIPE_ALL>` 偏粗（含 Fsm 包装与 PrefixEmbed）；**非死等** |
| 真死等红线 | **无**（无高严重度非 SYNC-03 项） |

## 3. encaps SIM（可选 1 轮）

| 项 | 值 |
|----|----|
| 目录 | `examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-encaps-k4` |
| 命令 | `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4`（未设 `F203_L18_TRACE`） |
| 并行 | 开跑前确认无 E20/其它 SIM 进程 |
| 结果 | **PASS** `rc=0`；`[verify] c.bin/K.bin max_abs_diff=0`；`[SUCCESS] … (sim)` |
| wall_sec | **107.035**（budget 900；非 124） |
| launch | prep ≈84.3s；`f203_encrypt_l18_l19` ≈22.3s；`sim_total_tick=726313` |
| stray dump | **无**（用例根） |
| 日志 | `/opt/cursor/artifacts/e21-encaps-sim1.log` |

## 4. 对图谱节点的 effect

| node | effect | note |
|------|--------|------|
| `J-hang-after-full-trace` | **support（结构）** | 源码钉死：全 CrossCore 结束后确有 AIV-only `mod_q`+`tail_pack` 窗 |
| `J-use-cannbot-on-gap` | **support** | 已跑 sync_audit；SYNC-03 记假阳性（同 E17） |
| `J-sim-not-sticky` | **cite / reaffirm** | encaps 默认路径 SIM×1 绿；本刀不证粘性，仅静态钉死零 CrossCore 尾窗 |
| `F-encrypt-gap-inventory` | **cite** | 末段缺口 = AIC 早退 + AIV 零 CrossCore 尾包 |

## 5. 范围合规

- 只读分析 + cannbot audit + 本 FEEDBACK；**未改** Encrypt / l18 kernel 业务码。
- 未开 `F203_L18_TRACE` 默认；未 NPU；未 commit/push。
- 与 E20：静态与 audit 先行；SIM 仅在确认无并行第二路后跑（或跳过）。
