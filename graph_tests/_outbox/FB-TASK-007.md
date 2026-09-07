# FEEDBACK TASK-007

## 结果摘要
- outcome: PASS
- one_liner: 落地 `fix-encrypt-clean-hostmu-2launch`（PHASE-P0）：Host 2-launch + **结构默认** Host μ + 设备 skipNtt **无 PrefixEmbed**；SIM 绿（wall≈4.75s，magic `CLNENC01`/`0x21`）→ **support** `D-next-clean-p0` / `F-host-mu-ok-sim` / Wait(4)↔SET(4)。

## 对图谱的影响（主控据此刷新；subagent 默认不改 yaml）
| node_id | effect | evidence_path |
|---------|--------|---------------|
| D-next-clean-p0 | **support** | 目录落地 + SIM PASS：`/opt/cursor/artifacts/task007_clean_hostmu_sim.log` |
| F-host-mu-ok-sim | **support** | Host 两 launch 间 `HostFoldMuAlways`（非开关）；设备 L2 无 PrefixEmbed；magic OK |
| J-empty-trace-aic-wait4 | **support**（可达性） | Launch-2 AIC 入口 `Wait(4)`；双 AIV 短 stub 后 `SET(4)`；SIM 未挂 |
| D-forbid-syncall-while-wait | **support** | Wait 路径无 SyncAll |
| D-softsync-follow-decrypt | n/a | 未引入自造 SoftSync |
| D-reject-correctness-antipattern | **support** | 固定 2 launch（prep+skipNtt），非滥增 |
| D-verify-sim-for-npu | **support** | 本线仅 SIM；未沉淀 CPU |
| docs/rg-kem-encrypt-hang.yaml | n/a | **未改** yaml（write_graph=no） |

## 实际改动
- files:
  - `ascendc-tests/fix-encrypt-clean-hostmu-2launch/`（完整探针：`main.cpp`/`mmad_custom.cpp`/`aiv_func.hpp`/`aic_func.hpp`/`tiling.h`/`run.sh`/`scripts/`/`STATUS.md`/`CMakeLists.txt`/`cmake/`）
  - `ascendc-tests/INDEX.md`（一行）
  - `graph_tests/_outbox/FB-TASK-007.md`（本文件）
- 注：TASK/用户文案写 `pass-encrypt-*`；仓库惯例与图谱/`AGENT_HANDOFF` 用 **`fix-encrypt-clean-hostmu-2launch`**（进行中 `fix-`），本单按后者落地。
- 未改：图谱 yaml；stable/Decaps；frozen；无 commit/push；无新增 AscendC API（复用已有 Mmad/DataCopy/CrossCore）

## 命令与关键日志
```bash
cd /workspace/ascendc-tests/fix-encrypt-clean-hostmu-2launch
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
# [clean-enc] Launch1 (prep+NTT) done
# [clean-enc] HostFoldMuAlways: e2Fold[0]=MU01 e2Fold[1]=1 (default, not optional)
# [clean-enc] Launch2 (skipNtt, no PrefixEmbed) done
# [INFO] Total tick: 22631
# [wall_sec] 4.747
# [kernel-run-timeout] wall_sec=4.747 budget=180s rc=0
# [SUCCESS] magic OK (64 B, prefix=b'CLNENC01', out[8]=0x21, HostFoldMuAlways + skipNtt no PrefixEmbed)
# [SUCCESS] fix-encrypt-clean-hostmu-2launch (sim)
# SIM_EXIT=0
```
完整日志：`/opt/cursor/artifacts/task007_clean_hostmu_sim.log`  
用例根无 stray `core*.dump`（dump 在 `sim_log/`）。

## SIM 墙钟（必填若跑了 sim）
- sim_sec: 4.747（kernel 段）
- vs_full_encaps: faster（全量 Encaps SIM ~200s 量级）

## 意外发现（新事实候选，勿写成长叙事）
- 干净树去掉可选 `SKEL_HOST_MU`/`SKIPNTT` 宏后，双 launch + HostFold 仍 SIM 绿；结构约束可独立于 skel 开关玩具验证。
- magic 前缀用 `CLNENC01`（与早期草稿 `CLNENC2L` 区分本实现）。

## 建议下一刀（可选，主控可不采纳）
- PHASE-P1：接入真实 at_jp / INTT / pack（`library/shared`），仍禁 PrefixEmbed、禁 frozen。
- 或与 stable Host μ 一并请用户 NPU 加压对照。
