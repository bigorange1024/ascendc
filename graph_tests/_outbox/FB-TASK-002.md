# FEEDBACK TASK-002

## 结果摘要
- outcome: PASS
- one_liner: 骨架增量 GATE CrossCore 4↔8（`SKEL_GATE` 默认 1）；SIM 仍绿（~4.05s），GATE alone 不足以逼出挂死。

## 对图谱的影响（主控据此刷新；subagent 默认不改 yaml）
| node_id | effect | evidence_path |
|---------|--------|---------------|
| J-hang-needs-extra-factor | weaken | 在已绿 1/3 双轮 stub 上插入 GATE 4↔8 后 SIM **仍正常结束**（wall 4.052s / tick 19639，非 124）→ 「GATE 即充分附加因子」削弱；挂死仍需更大/其它因子（真哈希、大 tiling、多轮 Cube、TRACE 等） |
| J-empty-trace-aic-wait4 | n/a | 本 toy **无 TRACE GM/poller**；AIC `Wait(4)` 后立即 `Set(8)` 放行，未复现「空 TRACE↔WAIT(4)」挂点；不能支持也不能证伪该节点 |
| Q-toy-repro | support | 骨架可加压 GATE 且 SIM 快结束（非挂）；继续开放「何种附加组合逼近实机挂」 |
| F-skel-toy-sim-pass | support | GATE 加压后仍 SIM PASS；基线开关 `SKEL_GATE=0` 亦绿 |
| F-gate-4-8 | support | 最小 GATE 子集已落地并可切换；与 Encrypt l18 at_jp 同构（双 AIV SET4 / AIC WAIT4→SET8 / AIV WAIT8） |
| D-next-stress-skel | support | GATE 加压完成；下一刀宜放大 MMAD / 真 LUT / TRACE，而非重复 GATE alone |
| D-forbid-syncall-while-wait | n/a | 实现中未使用 SyncAll |
| D-no-autonomous-push | support | 未 commit/push |

> 说明：本实验 **weaken**「stub + GATE alone 足以 SIM 挂」；**未 refute**「全量 Encrypt 路径上 GATE 与挂死共现」。未做可选 MMAD 放大（预算内优先交卷）。

## 实际改动
- files:
  - `ascendc-tests/fix-encrypt-skel-mix-chain-toy/mmad_custom.cpp`（GATE 4/8 + `SKEL_GATE`）
  - `ascendc-tests/fix-encrypt-skel-mix-chain-toy/aiv_func.hpp`（magic `out[8]` 标记）
  - `ascendc-tests/fix-encrypt-skel-mix-chain-toy/tiling.h` / `CMakeLists.txt` / `cmake/*.cmake` / `run.sh`
  - `ascendc-tests/fix-encrypt-skel-mix-chain-toy/scripts/verify_result.py` / `STATUS.md`
  - `graph_tests/INDEX.md`（一行）
  - `graph_tests/_outbox/FB-TASK-002.md`（本文件）
- 未改（说明）：`docs/rg-kem-encrypt-hang.yaml`（write_graph: no）；未动 stable/frozen；API 复用已查 `CrossCoreSetFlag`/`WaitFlag`，未改查阅索引（白名单外）

## 命令与关键日志
```bash
cd /workspace/ascendc-tests/fix-encrypt-skel-mix-chain-toy
SKEL_GATE=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
# 退出码 0
# 关键尾部：
[INFO] Model Start Time: 2026-09-03 10:11:16
[INFO] Model Stop Time: 2026-09-03 10:11:20
Model RUN TIME: 3476.57 ms
[INFO] Total tick: 19639
[INFO] Model stopped successfully.
[wall_sec] 4.052
[kernel-run-timeout] wall_sec=4.052 budget=180s rc=0 bin=ascendc_kernels_bbit
[SUCCESS] magic OK (64 B, prefix=b'SKELENC1', out[8]=0x04, SKEL_GATE=1)
[SUCCESS] fix-encrypt-skel-mix-chain-toy (sim)

# 对照：
SKEL_GATE=0 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
# 退出码 0；wall_sec=4.171 tick=19716；out[8]=0xa5；magic OK
```
完整日志：`/opt/cursor/artifacts/task002_skel_gate1_sim.log`、`/opt/cursor/artifacts/task002_skel_gate0_sim.log`

## SIM 墙钟（必填若跑了 sim）
- sim_sec: 4.052（GATE=1 kernel `wall_sec`）；对照 GATE=0：4.171
- vs_full_encaps: faster

## 意外发现（新事实候选，勿写成长叙事）
- GATE 插入后 tick（19639）与无 GATE（19716 / 基线 19663）同量级，无显著膨胀。
- 双 AIV 均 `SET(4)`、无 SoftSync、无 SyncAll，SIM 握手稳定。

## 建议下一刀（可选，主控可不采纳）
- 在本探针上：① 放大 MMAD（如 16×64×64 或连续四路 Cube）；② 或加空 TRACE 槽 + AIC Wait(4) 旁路观测，对准 `J-empty-trace-aic-wait4`；③ 勿再单测 GATE alone。
