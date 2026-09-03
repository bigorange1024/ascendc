# FEEDBACK TASK-006

## 结果摘要
- outcome: PASS
- one_liner: stable Encaps skipNtt 路径 Host 折 `e₂+=μ`（默认开）+ 设备跳过 PrefixEmbed；SIM 默认与 `F203_HOST_FOLD_MU=0` 对照均绿 → **support** `D-next-stable-host-mu` / `F-host-mu-ok-sim`；**建议用户上 NPU** 验证粘性。

## 对图谱的影响（主控据此刷新；subagent 默认不改 yaml）
| node_id | effect | evidence_path |
|---------|--------|---------------|
| D-next-stable-host-mu | **support** | Host `HostFoldMuIntoE2InPlace` + l18 `mGm=nullptr`；SIM verify PASS：`/opt/cursor/artifacts/task006_host_mu1_sim.log` |
| F-host-mu-ok-sim | **support** | 默认 Host 折 μ：`c/K max_abs_diff=0`，wall≈215s，rc=0 |
| J-real-aiv0-before-mu-mark | **weaken**（对「μ 前缀本身必挂」之 SIM 侧） | 设备跳过 PrefixEmbed/TR_AIV_MU_E2 后 SIM 仍绿；实机空 TRACE 仍待 NPU 证伪/证实 |
| J-empty-trace-aic-wait4 | n/a | 本单未再做 OMIT_SET4；握手路径保留 at_jp→SET(4) |
| D-goal-npu | **support**（候选就绪） | SIM 充分；建议用户代跑默认 `F203_HOST_FOLD_MU` Encaps 实机 |
| D-no-autonomous-push | support | 未 commit/push；未改 yaml；未改 Decaps |

## 实际改动
- files:
  - `examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-encaps-k4/main_kem_encaps.cpp`
    - `HostFoldMuEnabled`（默认开；仅 `=0` 关）
    - `HostFoldMuIntoE2InPlace`（对齐设备 μ←m + e₂+=μ mod q）
    - skipNtt 路径 l18 前 D2H/折/H2D；传 `mGm=nullptr`
  - `…/compute/f203_encrypt_l18_l19_kernel.cpp`
    - skipNtt + `mGm==nullptr` → 跳过 `PrefixEmbedMuIntoE2Gm` / `TR_AIV_MU_E2`
    - fused / `F203_HOST_FOLD_MU=0`（mGm 非空）仍设备前缀
  - `…/run.sh`（头注释：默认 Host 折；调试 `F203_HOST_FOLD_MU=0`）
  - `graph_tests/INDEX.md`（一行）
  - `graph_tests/_outbox/FB-TASK-006.md`（本文件）
- 未改：`docs/rg-kem-encrypt-hang.yaml`；Decaps / 其它树；无新增 AscendC API；无 commit/push

## 命令与关键日志
```bash
cd /workspace/examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-encaps-k4

# A 默认 Host 折 μ（FORCE_REBUILD 一次）
KEM_ENCAPS_FORCE_REBUILD=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
# wall_sec=214.878 budget=900s rc=0
# [kem-enc] F203_HOST_FOLD_MU=1：Host 已折 e2+=mu；l18 mGm=null
# [verify] c.bin/K.bin max_abs_diff=0 → [SUCCESS]

# B 调试回退设备 μ
F203_HOST_FOLD_MU=0 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
# wall_sec=201.597 budget=900s rc=0
# [kem-enc] F203_HOST_FOLD_MU=0：设备 PrefixEmbed μ（调试）
# [verify] PASS → [SUCCESS]
```
完整日志：
- `/opt/cursor/artifacts/task006_host_mu1_sim.log`
- `/opt/cursor/artifacts/task006_host_mu0_sim.log`

## SIM 墙钟（必填若跑了 sim）
- sim_sec: A(host μ)=214.878；B(device μ)=201.597
- vs_full_encaps: same-order（本即全量 Encaps SIM）

## 意外发现（新事实候选，勿写成长叙事）
- Host 折 μ 后 l18 tick/墙钟与设备 μ 同量级（SIM 噪声下 B 略快）；正确性两边均绿 → Host 折与设备 PrefixEmbed **I/O 等价**在 stable 上成立。
- 用例根无 stray `core*.dump`。

## 建议下一刀（可选，主控可不采纳）
- **请用户上 NPU**：同目录默认 `bash run.sh -r npu -v Ascend910B4`（Host 折 μ），看 `l18_l19` 是否仍粘 / TRACE 是否非空。
- 对照可选：`F203_HOST_FOLD_MU=0` 实机（旧设备 μ）对比粘性是否复现。
