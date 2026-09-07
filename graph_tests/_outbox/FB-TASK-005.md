# FEEDBACK TASK-005

## 结果摘要
- outcome: PASS
- one_liner: skipNtt 上 `SKEL_HOST_MU=1/0` 均 SIM 绿；OMIT_SET4 仍 124 → **support** `D-next-host-fold-mu`；Host 折 μ **不破坏** Wait(4) 握手。

## 三档 SIM 结果表

| 档 | 配置 | 结果 | wall_sec | tick / 备注 |
|----|------|------|----------|-------------|
| A Host 折 μ | `SKEL_SKIPNTT=1` `SKEL_HOST_MU=1` `SKEL_OMIT_SET4=0` | **绿** rc=0 | 2.848 | tick **11328**；`out[8]=0x15` |
| B 设备 μ-stub | `SKEL_SKIPNTT=1` `SKEL_HOST_MU=0` `SKEL_OMIT_SET4=0` | **绿** rc=0 | 2.654 | tick **12268**；`out[8]=0x14`（μ-stub 多 ~940 tick） |
| C 故障注入 | `SKIPNTT=1` `HOST_MU=1` `OMIT_SET4=1`；`KERNEL_COMPUTE_BUDGET_SEC=60` | **挂** rc=**124** | 60.354 | 无 magic SUCCESS |

判读：**A/B 绿** → Host 折 μ 与设备短 μ-stub 均可完成 SET(4)；**C 124** → 缺 SET 仍挂（回归 TASK-004）。

## 对图谱的影响（主控据此刷新；subagent 默认不改 yaml）
| node_id | effect | evidence_path |
|---------|--------|---------------|
| D-next-host-fold-mu | **support** | A 绿：Host launch 前写 STUB 占位、设备跳过 μ-stub、双 AIV 尽快 SET(4)；`/opt/cursor/artifacts/task005_skel_A_host_mu1_sim.log` |
| J-real-aiv0-before-mu-mark | **weaken**（对「μ 前缀本身必挂」） | B 绿：AIV0 做短 `StubPrefixEmbedMu`（GM↔UB）后仍 SET(4)；toy **未**复现「卡在 μ 标记前」空 TRACE；若实机卡在 μ 内，Host 折 μ（A）是可 SIM 验证的绕法 |
| F-omit-set4-hangs-sim | **support** | C 仍 124：`…/task005_skel_C_omit_set4_sim.log` |
| J-empty-trace-aic-wait4 | support | 入口 Wait(4) 机制仍成立（A/B 配对 SET 绿，C 缺 SET 挂） |
| Q-root-cause | n/a | 未闭环实机根因；仅给 Host 折 μ 改法 SIM 证据 |
| Q-toy-repro | support | 可切换 HOST_MU / OMIT_SET4 |
| D-no-autonomous-push | support | 未 commit/push；未改 yaml；未改 stable |

> Host 折 μ **不破坏** skipNtt 握手。迁 stable Encaps（**本单不改**）建议：Host 在 `l18_l19` launch 前完成 `e₂+=μ`（或等价预折）；设备删/跳过 `PrefixEmbedMuIntoE2Gm` + `TR_AIV_MU_E2`，保留双 AIV at_jp→`SET(4)` 与 AIC 入口 `Wait(4)`。

## 实际改动
- files:
  - `ascendc-tests/fix-encrypt-skel-mix-chain-toy/mmad_custom.cpp`（HOST_MU 分支）
  - `aiv_func.hpp`（`StubPrefixEmbedMu`）
  - `main.cpp`（`HostFoldMuPlaceholder`）
  - `tiling.h`（`kMagicHostMuMark` / `kHostMuFoldMark`）
  - `CMakeLists.txt` / `cmake/*.cmake` / `run.sh` / `scripts/verify_result.py` / `STATUS.md`
  - `ascendc-tests/INDEX.md`、`graph_tests/INDEX.md`
  - `graph_tests/_outbox/FB-TASK-005.md`（本文件）
- 未改（说明）：`docs/rg-kem-encrypt-hang.yaml`；stable Encaps 整树；无新增 AscendC API（复用 DataCopy/Adds/PipeBarrier）；未 commit/push

## 命令与关键日志
```bash
cd /workspace/ascendc-tests/fix-encrypt-skel-mix-chain-toy

# A Host 折 μ
SKEL_SKIPNTT=1 SKEL_HOST_MU=1 SKEL_OMIT_SET4=0 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
# wall_sec=2.848 budget=180s rc=0；magic OK out[8]=0x15 SKEL_HOST_MU=1

# B 设备 μ-stub
SKEL_SKIPNTT=1 SKEL_HOST_MU=0 SKEL_OMIT_SET4=0 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
# wall_sec=2.654 budget=180s rc=0；magic OK out[8]=0x14 SKEL_HOST_MU=0；tick 12268

# C（预算 60s）
KERNEL_COMPUTE_BUDGET_SEC=60 SKEL_SKIPNTT=1 SKEL_HOST_MU=1 SKEL_OMIT_SET4=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
# wall_sec=60.354 budget=60s rc=124；无 [SUCCESS] magic
```
完整日志：
- `/opt/cursor/artifacts/task005_skel_A_host_mu1_sim.log`
- `/opt/cursor/artifacts/task005_skel_B_host_mu0_sim.log`
- `/opt/cursor/artifacts/task005_skel_C_omit_set4_sim.log`

## SIM 墙钟（必填若跑了 sim）
- sim_sec: A=2.848；B=2.654；C=60.354（timeout）
- vs_full_encaps: faster（A/B）

## 意外发现（新事实候选，勿写成长叙事）
- 设备 μ-stub tick（12268）> Host 折 μ（11328），差约 940 tick；墙钟相近（SIM 噪声）。
- 轻量 μ-stub **不足以**在 SIM 复现「卡在 μ 前」；实机空 TRACE 仍需更重 PrefixEmbed 或其它阻塞因子。

## 建议下一刀（可选，主控可不采纳）
- 用户代跑：stable Encaps Host 折 μ（删设备 PrefixEmbed）实机验证。
- 或在 toy 加重 μ-stub（接近真 256 系数 + decode）再测是否拖住 SET(4)。
