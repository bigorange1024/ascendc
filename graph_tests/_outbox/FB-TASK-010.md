# FEEDBACK TASK-010

## 结果摘要
- outcome: PARTIAL
- one_liner: `SKEL_OMIT_SLOT0` 已落地且与 OMIT_SET4 互斥；默认 SIM 仍绿（magic `SKELDEC1`，wall≈3.2s）；OMIT_SLOT0=1 预算60s **未挂**（rc=0，wall≈3.6s）→ **weaken** `J-omit-slot0-spin-hangs`（未改参硬凑）。

## 对图谱的影响（主控据此刷新；subagent 默认不改 yaml）
| node_id | effect | evidence_path |
|---------|--------|---------------|
| J-omit-slot0-spin-hangs | weaken | `/opt/cursor/artifacts/decrypt-skel-toy-slot0-B.log`：`SKEL_OMIT_SLOT0=1` cmake `-- SKEL_OMIT_SLOT0=1`；device `CXX_DEFINES` 含 `-DSKEL_OMIT_SLOT0=1`；kernel `wall_sec=3.632` `rc=0` 且 magic 绿——AIV0 省略 slot0 写哨兵**未**在本 toy/SIM 上复现 124 |
| F-decrypt-skel-legal-sim-pass | support | `/opt/cursor/artifacts/decrypt-skel-toy-slot0-A.log`：`wall_sec=3.223` `rc=0` magic `SKELDEC1`/`0x04`（加开关后默认仍绿） |
| F-softsync-two-slots | weaken | 同 B：省略 Arrive(slot0) 后链仍跑完；说明本 toy 的 `while(s[0]==0)` 在 CAModel SIM 上**不足以**造成可观测挂死（编译器可能删空自旋 / 或 AIV1 自旋不挡 Model Stop） |
| D-next-omit-slot0 | support | 开关 + 双档 SIM 已跑完；本 decision 可收 |
| Q-hang-which-layer | support | SoftSync 前置层在本实验**未能**单独制造 124；对比 TASK-009 缺 SET(4) **能** 124 → 挂更贴近 CrossCore GATE，而非「仅忘写 slot0 哨兵」于当前 SIM 可见 |
| Q-toy-repro | n/a | toy 仍可绿；slot0-omit 档未能作 hang 复现器 |

> **失败优先**：B/C 档预期 124 **未出现**；上表已写 weaken，禁止改预算/形状硬凑。

## 实际改动
- files:
  - `ascendc-tests/fix-decrypt-skel-mix-chain-toy/mmad_custom.cpp`（`SKEL_OMIT_SLOT0`：slot0 时 AIV0 Arrive 空操作；注释标明 SoftSync 为 SET(4) 前置）
  - `…/CMakeLists.txt`、`cmake/npu_lib.cmake`、`cmake/cpu_lib.cmake`（`-DSKEL_OMIT_SLOT0=`）
  - `…/run.sh`（env→cmake；与 `SKEL_OMIT_SET4=1` 互斥报错）
  - `…/tiling.h`、`STATUS.md`
  - `graph_tests/INDEX.md`（TASK-010 一行）
  - `graph_tests/_outbox/FB-TASK-010.md`（本文件）
- 未改（说明）：未改图谱 yaml；未 commit/push；未开 OMIT_SET4；未改参硬凑 124

## 命令与关键日志
```bash
cd /workspace/ascendc-tests/fix-decrypt-skel-mix-chain-toy
SKEL_OMIT_SLOT0=0 SKEL_OMIT_SET4=0 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
# A_EXIT=0
# SKEL_OMIT_SET4=0 SKEL_OMIT_SLOT0=0
# -- SKEL_OMIT_SLOT0=0
# [INFO] Model Stop Time: 2026-09-03 16:58:52
# Model RUN TIME: 2882.16 ms
# [INFO] Total tick: 20815
# [INFO] Model stopped successfully.
# [wall_sec] 3.223
# [kernel-run-timeout] wall_sec=3.223 budget=180s rc=0 bin=ascendc_kernels_bbit
# [SUCCESS] magic OK (64 B, prefix=b'SKELDEC1', out[8]=0x04, SKEL_OMIT_SET4=0)
# [SUCCESS] fix-decrypt-skel-mix-chain-toy (sim)

KERNEL_COMPUTE_BUDGET_SEC=60 SKEL_OMIT_SLOT0=1 SKEL_OMIT_SET4=0 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
# B_EXIT=0   # 预期 124，实际未挂
# SKEL_OMIT_SET4=0 SKEL_OMIT_SLOT0=1
# -- SKEL_OMIT_SLOT0=1
# [INFO] Model Stop Time: 2026-09-03 16:59:08
# Model RUN TIME: 3188.6 ms
# [INFO] Total tick: 20216
# [INFO] Model stopped successfully.
# [wall_sec] 3.632
# [kernel-run-timeout] wall_sec=3.632 budget=60s rc=0 bin=ascendc_kernels_bbit
# [SUCCESS] magic OK (64 B, prefix=b'SKELDEC1', out[8]=0x04, SKEL_OMIT_SET4=0)
# [SUCCESS] fix-decrypt-skel-mix-chain-toy (sim)
```

完整日志：`/opt/cursor/artifacts/decrypt-skel-toy-slot0-A.log` · `/opt/cursor/artifacts/decrypt-skel-toy-slot0-B.log`  
用例根无 stray `core*.dump`。

## SIM 墙钟（必填若跑了 sim）
- sim_sec: A=3.223；B/C(OMIT_SLOT0)=3.632（budget=60，**未** timeout）
- vs_full_encaps: faster

## 意外发现（新事实候选，勿写成长叙事）
- device `flags.make` 确认 `-DSKEL_OMIT_SLOT0=1`；预处理可见 `if (slot == 0) return` 省略写哨兵——**实现已生效**，非宏未接线。
- 省略 slot0 后 tick/wall 与默认同量级仍绿：SIM 上 `while(s[0]==0)` 空自旋**未**转化为可观测 hang（候选：编译器删无副作用自旋，或 AIV1 自旋不阻挡 Model Stop）。
- TASK-009 缺 SET(4) 仍可 124 → CrossCore GATE 层 hang 机制与 SoftSync 层在 SIM 上可分离。

## 建议下一刀（可选，主控可不采纳）
- 若仍要证 SoftSync 层 hang：需**非改参**的可见副作用自旋（如带 GM 读 intrinsic / 计数写回）再测——须新 TASK；本单禁止硬凑。
- 或转向 T3：第一轮 Cube 后缺 slot1 / 第二轮 SET(4)。
- 仍勿请用户上机全量 Decrypt。
