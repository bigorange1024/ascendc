# FEEDBACK TASK-004

## 结果摘要
- outcome: PASS
- one_liner: skipNtt 入口 AIC Wait(4) 落地；B（SET4）SIM 绿；C（OMIT_SET4）budget=60s 超时 **124** → **support** `J-empty-trace-aic-wait4`。

## 三档 SIM 结果表

| 档 | 配置 | 结果 | wall_sec | tick / 备注 |
|----|------|------|----------|-------------|
| A 基线 | `SKEL_SKIPNTT=0`（默认 GATE=1） | **绿** rc=0 | 3.224 | tick 20140；`out[8]=0x04` |
| B skipNtt 正常 | `SKEL_SKIPNTT=1` `SKEL_OMIT_SET4=0` | **绿** rc=0 | 2.242 | tick 11399；`out[8]=0x14` |
| C 故障注入 | `SKEL_SKIPNTT=1` `SKEL_OMIT_SET4=1`；**`KERNEL_COMPUTE_BUDGET_SEC=60`**（临时降预算） | **挂** rc=**124** | 60.078 | Model 跑满预算后被 timeout 杀；无 magic SUCCESS |

判读：**B 绿 + C 挂** → SIM 可复现「AIC 入口 Wait(4) 且 AIV 不 SET(4) → 死等」机制。

## 对图谱的影响（主控据此刷新；subagent 默认不改 yaml）
| node_id | effect | evidence_path |
|---------|--------|---------------|
| J-empty-trace-aic-wait4 | **support** | B 绿 + C 124：`/opt/cursor/artifacts/task004_skel_B_skipntt_sim.log`、`…_C_omit_set4_sim.log`；机制「入口 Wait(4) 缺 SET」在 SIM 可复现 |
| Q-toy-repro | support | 骨架可切换 skipNtt / OMIT_SET4；B 快结束、C 可控挂死 |
| D-next-skipntt-wait4 | support | 本单完成：入口 Wait(4) + 对照缺 SET |
| F-trace-empty-0-16 | n/a | 本 toy **仍无 TRACE GM/poller**；仅验证 Wait(4) 死等半边，未复现「空 TRACE 槽」本身 |
| F-gate-4-8 | n/a | skipNtt 路径未用中段 GATE 4↔8（flag 4 专用于入口） |
| J-hang-needs-extra-factor | support | 在已绿 GATE/Cube 骨架上，**缺 SET(4)** 即足以 SIM 挂 → 附加因子候选「未配对 Wait(4)」成立（stub 下） |
| D-forbid-syncall-while-wait | n/a | 未使用 SyncAll |
| D-no-autonomous-push | support | 未 commit/push |

> 说明：本实验 **support**「skipNtt 入口 AIC Wait(4) 缺配对 SET → 挂死」在 SIM 可复现；**未**证明实机 Encrypt 即此根因（无 TRACE / 无真 l18）；亦未 refute 其它附加因子。

## 实际改动
- files:
  - `ascendc-tests/fix-encrypt-skel-mix-chain-toy/mmad_custom.cpp`（`SKEL_SKIPNTT` / `SKEL_OMIT_SET4`）
  - `aiv_func.hpp`（`StubEncodeMagic` 改 markB8）
  - `tiling.h`（`kMagicSkipNttMark=0x14`）
  - `CMakeLists.txt` / `cmake/*.cmake` / `run.sh` / `scripts/verify_result.py` / `STATUS.md`
  - `graph_tests/_outbox/FB-TASK-004.md`（本文件）
- 未改（说明）：`docs/rg-kem-encrypt-hang.yaml`（write_graph: no）；未动 stable/frozen；无新增 AscendC API；未 commit/push

## 命令与关键日志
```bash
cd /workspace/ascendc-tests/fix-encrypt-skel-mix-chain-toy

# A
SKEL_SKIPNTT=0 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
# wall_sec=3.224 budget=180s rc=0；magic OK out[8]=0x04 SKEL_SKIPNTT=0

# B
SKEL_SKIPNTT=1 SKEL_OMIT_SET4=0 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
# wall_sec=2.242 budget=180s rc=0；magic OK out[8]=0x14 SKEL_SKIPNTT=1

# C（预算临时 60s）
SKEL_SKIPNTT=1 SKEL_OMIT_SET4=1 KERNEL_COMPUTE_BUDGET_SEC=60 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
# Model Start … Stop 间隔 ~60s；wall_sec=60.078 budget=60s rc=124
# 无 [SUCCESS] magic / toy
```
完整日志：
- `/opt/cursor/artifacts/task004_skel_A_baseline_sim.log`
- `/opt/cursor/artifacts/task004_skel_B_skipntt_sim.log`
- `/opt/cursor/artifacts/task004_skel_C_omit_set4_sim.log`

## SIM 墙钟（必填若跑了 sim）
- sim_sec: A=3.224；B=2.242；C=60.078（timeout）
- vs_full_encaps: faster（A/B）；C 为受控挂死

## 意外发现（新事实候选，勿写成长叙事）
- skipNtt 正常路径 tick≈11399，显著轻于默认 GATE 双阶段（≈20140），因省略中段 GATE + 后半 INTT。
- C 档 CAModel 在 timeout 杀进程前仍打印 `Model Stop Time`（被 60s 预算截断），非瞬崩。

## 建议下一刀（可选，主控可不采纳）
- 在 skipNtt 骨架上加空 TRACE 槽 / poller，把 `F-trace-empty-0-16` 与 Wait(4) 绑死观测；或对照「仅缺 SET8」是否同类挂。
- 勿再无限加压 Cube alone；实机验证仍待用户代跑。
