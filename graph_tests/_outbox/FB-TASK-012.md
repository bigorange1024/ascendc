# FEEDBACK TASK-012

## 结果摘要
- outcome: PASS
- one_liner: Host `SKEL_SOFTSYNC_PREFILL` 0/1 双档 SIM **均绿**（wall≈3.86s / 4.08s）→ **support** `J-dirty-softsync-hang-vs-race`（脏哨兵更像误放行，**不是** SIM hang 充分条件）；未改设备 SoftSync、未开 OMIT_*、未硬凑。

## 对图谱的影响（主控据此刷新；subagent 默认不改 yaml）
| node_id | effect | evidence_path |
|---------|--------|---------------|
| J-dirty-softsync-hang-vs-race | support | `/opt/cursor/artifacts/decrypt-skel-toy-dirty0.log` + `decrypt-skel-toy-dirty1.log`：PREFILL=0 → `wall_sec=3.860` `rc=0`；PREFILL=1 → `wall_sec=4.078` `rc=0`；两档 magic `SKELDEC1`/`0x04` —— 脏非零≠hang，更像 AIV1 误放行 |
| F-host-zeros-softsync | support | dirty0：默认清零路径仍绿；生产定式未破 |
| J-sim-empty-gm-spin-not-hang | support | 与 TASK-010 同向：SoftSync 层（空自旋 / 脏预填）均**不能**稳定造 SIM 124 |
| D-next-dirty-softsync | support | Host 开关 + 双档 SIM 已跑完；本 decision 可收 |
| Q-hang-which-layer | support | 脏 softSync **不**挂 → 进一步把可靠 hang 代理收窄到 CrossCore GATE 缺 SET(4)（TASK-009/011），而非 SoftSync 哨兵值 |

> **失败优先**：本单**不**期望 124；两档均绿即达标。未出现挂 → 无需 weaken「预期」；weaken 的是「脏 GM=hang」旧直觉。

## 实际改动
- files:
  - `ascendc-tests/fix-decrypt-skel-mix-chain-toy/main.cpp`（运行时读 `SKEL_SOFTSYNC_PREFILL`；0 清零 / 1 写 int32[2]=1）
  - `…/run.sh`（export 默认 0；头注释 Usage；echo 打印）
  - `…/STATUS.md`（档 E + TASK-012 验收表）
  - `graph_tests/INDEX.md`（TASK-012 → PASS）
  - `graph_tests/_outbox/FB-TASK-012.md`（本文件）
- 未改（说明）：未改设备 `mmad_custom.cpp` / SoftSync 空 while；未开任何 OMIT_*；未改图谱 yaml；未 commit/push
- 目录名：活跃用例为 `fix-decrypt-skel-mix-chain-toy/`（TASK 文中 `pass-…` 为笔误）

## 命令与关键日志
```bash
cd /workspace/ascendc-tests/fix-decrypt-skel-mix-chain-toy
SKEL_SOFTSYNC_PREFILL=0 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
# EXIT0=0
# SKEL_OMIT_SET4=0 SKEL_OMIT_SLOT0=0 SKEL_OMIT_SET4_R2=0 SKEL_SOFTSYNC_PREFILL=0
# SKEL_SOFTSYNC_PREFILL=0
# Model RUN TIME: 3283.18 ms
# [wall_sec] 3.860
# [kernel-run-timeout] wall_sec=3.860 budget=180s rc=0 bin=ascendc_kernels_bbit
# [SUCCESS] magic OK (64 B, prefix=b'SKELDEC1', out[8]=0x04, SKEL_OMIT_SET4=0)
# [SUCCESS] fix-decrypt-skel-mix-chain-toy (sim)

SKEL_SOFTSYNC_PREFILL=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
# EXIT1=0
# SKEL_OMIT_SET4=0 SKEL_OMIT_SLOT0=0 SKEL_OMIT_SET4_R2=0 SKEL_SOFTSYNC_PREFILL=1
# SKEL_SOFTSYNC_PREFILL=1
# Model RUN TIME: 3260.88 ms
# [wall_sec] 4.078
# [kernel-run-timeout] wall_sec=4.078 budget=180s rc=0 bin=ascendc_kernels_bbit
# [SUCCESS] magic OK (64 B, prefix=b'SKELDEC1', out[8]=0x04, SKEL_OMIT_SET4=0)
# [SUCCESS] fix-decrypt-skel-mix-chain-toy (sim)
```

完整日志：`/opt/cursor/artifacts/decrypt-skel-toy-dirty0.log` · `/opt/cursor/artifacts/decrypt-skel-toy-dirty1.log`  
用例根无 stray `core*.dump`。

## SIM 墙钟（必填若跑了 sim）
- sim_sec: dirty0(PREFILL=0)=3.860；dirty1(PREFILL=1)=4.078（budget=180，**均 rc=0**）
- vs_full_encaps: faster（骨架 toy）

## 意外发现（新事实候选，勿写成长叙事）
- 脏预填与清零 wall 几乎同量级（~4s），无「半挂」迹象。
- PREFILL=1 不破坏 magic/`0x04`：AIV0 仍写 Arrive/Clear，GATE 仍齐步；脏值只跳过 AIV1 自旋等待。

## 建议下一刀（可选，主控可不采纳）
- 主控刷新 yaml：`J-dirty-softsync-hang-vs-race` → support；收 `D-next-dirty-softsync`。
- Hang 复现继续用 CrossCore OMIT_SET4 / OMIT_SET4_R2；勿再把 SoftSync 脏/空 while 当 hang 代理。
