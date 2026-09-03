# FEEDBACK TASK-011

## 结果摘要
- outcome: PASS
- one_liner: `SKEL_OMIT_SET4_R2` 落地（默认绿 wall≈3.4s；R2=1 预算60s → **rc=124**）→ **support** `J-omit-slot1-hangs-after-ntt`；未改空 while / 未硬凑。

## 对图谱的影响（主控据此刷新；subagent 默认不改 yaml）
| node_id | effect | evidence_path |
|---------|--------|---------------|
| J-omit-slot1-hangs-after-ntt | support | `/opt/cursor/artifacts/decrypt-skel-toy-r2-B.log`：`SKEL_OMIT_SET4_R2=1` cmake `-- SKEL_OMIT_SET4_R2=1`；device `CXX_DEFINES` 含 `-DSKEL_OMIT_SET4_R2=1`；`wall_sec=60.103` `rc=124`（budget=60）——第一轮合法 SET(4) 后第二轮省略 → AIC 第二段 Wait(4) 挂 |
| F-decrypt-omit-set4-hangs-sim | support | 同 B：与 TASK-009「两轮都不 SET(4)」同属 CrossCore GATE 缺 SET(4)⇒124；本单细化为「仅第二轮」仍挂 |
| F-decrypt-skel-legal-sim-pass | support | `/opt/cursor/artifacts/decrypt-skel-toy-r2-A.log`：`wall_sec=3.439` `rc=0` magic `SKELDEC1`/`0x04`（加开关后默认仍绿） |
| D-next-omit-set4-r2 | support | 开关 + 双档 SIM 已跑完；本 decision 可收 |
| J-sim-empty-gm-spin-not-hang | n/a | 本单**未**改 SoftSync 空 while；仍以 TASK-010 为准 |
| Q-hang-which-layer | support | 第二轮缺 SET(4) 可单独制造 124 → 挂贴近 **CrossCore GATE 第二段**，与 SoftSync 空自旋层可分离 |
| Q-toy-repro | support | R2 档可作为「第一轮 Cube 后缺 GATE」hang 复现器 |

> **失败优先**：B 档预期 124 **已出现**；无需 weaken。禁止硬凑。

## 实际改动
- files:
  - `ascendc-tests/fix-decrypt-skel-mix-chain-toy/mmad_custom.cpp`（`SKEL_OMIT_SET4_R2`：slot0 仍 SET(4)；slot1 跳过 SET(4)）
  - `…/CMakeLists.txt`、`cmake/npu_lib.cmake`、`cmake/cpu_lib.cmake`（`-DSKEL_OMIT_SET4_R2=`；删无用 `SKEL_GATE/HEAVY/SKIPNTT/HOST_MU` 空宏）
  - `…/run.sh`（env→cmake；与 `SKEL_OMIT_SET4=1`、`SKEL_OMIT_SLOT0=1` 三者互斥）
  - `…/tiling.h`、`STATUS.md`
  - `graph_tests/INDEX.md`（TASK-011 一行）
  - `graph_tests/_outbox/FB-TASK-011.md`（本文件）
- 未改（说明）：未改图谱 yaml；未 commit/push；未改空 while / volatile；未开 OMIT_SET4 / OMIT_SLOT0
- 目录名：活跃用例为 `fix-decrypt-skel-mix-chain-toy/`（TASK 文中 `pass-…` 为笔误；本单改的是既有 `fix-` 树）

## 命令与关键日志
```bash
cd /workspace/ascendc-tests/fix-decrypt-skel-mix-chain-toy
SKEL_OMIT_SET4_R2=0 SKEL_OMIT_SET4=0 SKEL_OMIT_SLOT0=0 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
# A_EXIT=0
# SKEL_OMIT_SET4=0 SKEL_OMIT_SLOT0=0 SKEL_OMIT_SET4_R2=0
# -- SKEL_OMIT_SET4_R2=0
# Model RUN TIME: 3081.68 ms
# [INFO] Total tick: 20839
# [wall_sec] 3.439
# [kernel-run-timeout] wall_sec=3.439 budget=180s rc=0 bin=ascendc_kernels_bbit
# [SUCCESS] magic OK (64 B, prefix=b'SKELDEC1', out[8]=0x04, SKEL_OMIT_SET4=0)
# [SUCCESS] fix-decrypt-skel-mix-chain-toy (sim)

KERNEL_COMPUTE_BUDGET_SEC=60 SKEL_OMIT_SET4_R2=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
# B_EXIT=124
# SKEL_OMIT_SET4=0 SKEL_OMIT_SLOT0=0 SKEL_OMIT_SET4_R2=1
# -- SKEL_OMIT_SET4_R2=1
# Model RUN TIME: 59794 ms
# [wall_sec] 60.103
# [kernel-run-timeout] wall_sec=60.103 budget=60s rc=124 bin=ascendc_kernels_bbit
```

完整日志：`/opt/cursor/artifacts/decrypt-skel-toy-r2-A.log` · `/opt/cursor/artifacts/decrypt-skel-toy-r2-B.log`  
用例根无 stray `core*.dump`。device flags 确认 `-DSKEL_OMIT_SET4_R2=1`。

## SIM 墙钟（必填若跑了 sim）
- sim_sec: A=3.439；B(OMIT_SET4_R2)=60.103（budget=60，**timeout 124**）
- vs_full_encaps: faster（合法档）/ hang-by-design（R2）

## 意外发现（新事实候选，勿写成长叙事）
- 仅第二轮缺 SET(4) 即可 124，说明 AIC **第一段 Wait(4) 已被合法放行**并完成 NTT Cube；挂点在第二段 GATE，而非入口。
- 与 TASK-010 SoftSync 空 while **不挂**对照：本 toy 上 CrossCore 缺 SET(4) 是可靠 SIM hang 代理，空 GM 自旋不是。

## 建议下一刀（可选，主控可不采纳）
- T4：dirty SoftSync 预填 vs 清零。
- 或把 R2 复现器对照生产第二段 GATE 路径做静态审计（仍勿请用户上机全量 Decrypt）。
