# FEEDBACK TASK-003

## 结果摘要
- outcome: PASS
- one_liner: `SKEL_HEAVY=1`（16×64×64 + 4 轮 flag 1/3，GATE 仍开）SIM **仍绿**（wall ~8.8s），大 Cube 负荷 alone 在 stub 下不足以致挂。

## 对图谱的影响（主控据此刷新；subagent 默认不改 yaml）
| node_id | effect | evidence_path |
|---------|--------|---------------|
| J-hang-needs-extra-factor | weaken | stub+GATE+放大/增多 Cube 后 SIM **正常结束**（HEAVY=1 wall 8.765s / tick 54416，非 124）→ 「附加因子=更大 Cube 负荷」在 stub 路径上削弱；挂死仍需其它因子（真哈希/LUT、TRACE 空转、接缝等） |
| J-common-mix-flag13 | weaken | 同构 1/3 增至 4 轮 + 更大 MMAD 仍不挂 → 再证 1/3 非充分挂因（与 TASK-001 一致，加压后仍成立） |
| Q-toy-repro | support | 骨架可加压 Cube 且 SIM 快结束；继续开放「何种组合逼近实机挂」 |
| F-skel-gate-sim-pass | support | HEAVY=0/1 均在 `SKEL_GATE=1` 下 SIM PASS |
| D-next-scale-mmad | support | 本单完成：`SKEL_HEAVY` 可切换；HEAVY=1 未挂 → 本刀关闭，勿再无限加压 |
| D-forbid-syncall-while-wait | n/a | 未使用 SyncAll |
| D-no-autonomous-push | support | 未 commit/push |

> 说明：本实验 **weaken**「stub 下更大/更多 Cube 即充分挂因」；**未 refute**「全量 Encrypt 路径上大 MMAD 与挂死共现」。按 TASK：仍绿即交卷，**不开第三档加压**。

## 实际改动
- files:
  - `ascendc-tests/fix-encrypt-skel-mix-chain-toy/tiling.h`（HEAVY→16×64×64；`kCubeRoundsPerPhase`）
  - `ascendc-tests/fix-encrypt-skel-mix-chain-toy/mmad_custom.cpp`（多轮 1/3 循环；HEAVY 注释选型）
  - `ascendc-tests/fix-encrypt-skel-mix-chain-toy/aic_func.hpp` / `aiv_func.hpp` / `main.cpp`（注释/尺寸说明）
  - `CMakeLists.txt` / `cmake/*.cmake` / `run.sh`（`SKEL_HEAVY`）
  - `scripts/gen_data.py` / `scripts/verify_result.py`
  - `STATUS.md`；`graph_tests/INDEX.md`（一行）
  - `graph_tests/_outbox/FB-TASK-003.md`（本文件）
- 未改（说明）：`docs/rg-kem-encrypt-hang.yaml`（write_graph: no）；未动 stable/frozen；无新增 AscendC API；未 commit/push

## 命令与关键日志
```bash
cd /workspace/ascendc-tests/fix-encrypt-skel-mix-chain-toy
SKEL_HEAVY=1 SKEL_GATE=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
# 退出码 0
# 关键尾部：
[INFO] Model Start Time: 2026-09-03 10:18:49
[INFO] Model Stop Time: 2026-09-03 10:18:58
Model RUN TIME: 8306.78 ms
[INFO] Total tick: 54416
[INFO] Model stopped successfully.
[wall_sec] 8.765
[kernel-run-timeout] wall_sec=8.765 budget=180s rc=0 bin=ascendc_kernels_bbit
[SUCCESS] magic OK (64 B, prefix=b'SKELENC1', out[8]=0x04, SKEL_GATE=1, SKEL_HEAVY=1)
[SUCCESS] fix-encrypt-skel-mix-chain-toy (sim)

# 对照：
SKEL_HEAVY=0 SKEL_GATE=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
# 退出码 0；wall_sec=3.402 tick=20082；magic OK SKEL_HEAVY=0
```
完整日志：`/opt/cursor/artifacts/task003_skel_heavy1_sim.log`、`/opt/cursor/artifacts/task003_skel_heavy0_sim.log`

## SIM 墙钟（必填若跑了 sim）
- sim_sec: 8.765（HEAVY=1）；对照 HEAVY=0：3.402
- vs_full_encaps: faster

## 意外发现（新事实候选，勿写成长叙事）
- HEAVY=1 tick≈54416 约为 HEAVY=0（≈20082）的 ~2.7×，负荷确实上去了但仍远未挂死（budget 180s）。
- 加压选型：**同时**放大单次 MMAD（16×64×64）与增多握手（4 轮 1/3），仍 1 MIX launch + stub 哈希。

## 建议下一刀（可选，主控可不采纳）
- 勿再单测更大 Cube alone；转向真 LUT/哈希片段、或空 TRACE + AIC Wait(4)（`J-empty-trace-aic-wait4`）、或 Encrypt 接缝（prep∈MIX / 多 launch）假说。
