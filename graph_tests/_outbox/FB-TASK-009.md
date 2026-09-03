# FEEDBACK TASK-009

## 结果摘要
- outcome: PASS
- one_liner: Decrypt 握手 toy A 合法 SoftSync+双 GATE+stub Cube SIM 绿（magic `SKELDEC1`/`0x04`，wall≈3.9s）；B `SKEL_OMIT_SET4=1` 预算60s 得 rc=124，支持缺 SET(4)⇒挂。

## 对图谱的影响（主控据此刷新；subagent 默认不改 yaml）
| node_id | effect | evidence_path |
|---------|--------|---------------|
| J-omit-set4-hangs-decrypt | support | `/opt/cursor/artifacts/decrypt-skel-toy-B.log`：`wall_sec=60.393` `rc=124`；合法 SoftSync 后两轮都不 SET(4) |
| J-hang-needs-handshake-break | support | A 绿 + B 124：合法 stub 链可跑完；挂来自握手断裂（缺 SET(4)）而非「双 Cube 本身」 |
| Q-toy-repro | support | `ascendc-tests/fix-decrypt-skel-mix-chain-toy/`：最小骨架可绿 / 可 124 |
| D-next-build-decrypt-toy | support | 目录已建；STATUS + INDEX 已记 |
| D-near-toy | support | SoftSync+GATE+stub Cube；不对正确性；SIM 门禁 |
| D-sim-enough-before-npu | n/a | 本单未建议上机；T0–T1 已沉积，后续 T2+ 仍属 SIM |
| D-softsync-is-prod-not-homemade | support | `mmad_custom.cpp` SoftSyncArrive：AIV0 写1、AIV1 自旋 |
| D-forbid-syncall-while-wait | support | AIC Wait 路径无 SyncAll |
| D-reject-correctness-antipattern | n/a | 未引入逐步外搬/滥 launch |
| J-dual-cube-sufficient-hang | n/a | 保持 retracted；本实验不复活该假说 |

> **失败优先**：B 档 124 为本假说**成功证据**，非失败。

## 实际改动
- files:
  - `ascendc-tests/fix-decrypt-skel-mix-chain-toy/`（新建整树：`mmad_custom.cpp`/`main.cpp`/`tiling.h`/`aiv_func.hpp`/`aic_func.hpp`/`CMakeLists.txt`/`run.sh`/`scripts/*`/`data_utils.h`/`STATUS.md` + `cmake/` 壳）
  - `ascendc-tests/INDEX.md`（非 ML-KEM 表一行）
  - `graph_tests/INDEX.md`（TASK-009 状态一行）
  - `graph_tests/_outbox/FB-TASK-009.md`（本文件）
- 未改（说明）：未改图谱 yaml；未改 `examples/stable/**`；未 commit/push；未动 AscendC API 查阅索引（仅用已有 CrossCore/PipeBarrier/DataCopy/Duplicate/Adds/`AicMmad`）

## 命令与关键日志
```bash
cd /workspace/ascendc-tests/fix-decrypt-skel-mix-chain-toy
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
# A_EXIT=0
# [INFO] Model Stop Time: 2026-09-03 16:52:24
# Model RUN TIME: 3254.72 ms
# [INFO] Total tick: 20735
# [INFO] Model stopped successfully.
# [wall_sec] 3.934
# [kernel-run-timeout] wall_sec=3.934 budget=180s rc=0 bin=ascendc_kernels_bbit
# [SUCCESS] magic OK (64 B, prefix=b'SKELDEC1', out[8]=0x04, SKEL_OMIT_SET4=0)
# [SUCCESS] fix-decrypt-skel-mix-chain-toy (sim)

KERNEL_COMPUTE_BUDGET_SEC=60 SKEL_OMIT_SET4=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
# B_EXIT=124
# [INFO] Model Stop Time: 2026-09-03 16:55:01
# Model RUN TIME: 59774.2 ms
# [wall_sec] 60.393
# [kernel-run-timeout] wall_sec=60.393 budget=60s rc=124 bin=ascendc_kernels_bbit
# [kernel-run-timeout] budget 60s exceeded (exit 124)。…
```

完整日志：`/opt/cursor/artifacts/decrypt-skel-toy-A.log` · `/opt/cursor/artifacts/decrypt-skel-toy-B.log`  
用例根无 stray `core*.dump`（dump 在 `sim_log/`）。

## SIM 墙钟（必填若跑了 sim）
- sim_sec: A=3.934；B=60.393（budget=60，timeout）
- vs_full_encaps: faster

## 意外发现（新事实候选，勿写成长叙事）
- 首编缺抄 `data_utils.h`（工程壳遗漏）导致 A 首次 cmake 失败；补文件后一次 SIM 绿，**非**同步逻辑返工。
- SoftSync 合法 + 缺 SET(4) 即可在 Decrypt 拓扑上复现 Encrypt 线同构的 SIM 124。

## 建议下一刀（可选，主控可不采纳）
- T2：`J-omit-slot0-spin-hangs`（AIV0 不 Arrive(slot0)）
- T3：第一轮 Cube 后缺 slot1 / 第二轮 SET(4)
- 仍勿请用户上机全量 Decrypt
