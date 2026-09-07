# TASK-005

## 元数据
- task_id: TASK-005
- issued_at: 2026-09-03T10:28Z
- deadline_min: 40
- max_retries: 1
- silent_hang_min: 10
- abort_on: [timeout, no_progress, scope_breach, sim_hang]
- graph: docs/rg-kem-encrypt-hang.yaml
- related_nodes: [J-real-aiv0-before-mu-mark, J-empty-trace-aic-wait4, D-next-host-fold-mu, F-omit-set4-hangs-sim, Q-root-cause]
- hypothesis_under_test: [J-real-aiv0-before-mu-mark, D-next-host-fold-mu]
- write_graph: no
- concurrency: solo

## 目标（一句话）
在 toy 的 skipNtt 路径上落地 **Host 折 μ / 设备跳过 PrefixEmbed** 原型并 SIM 验证；对照设备侧保留 μ-stub；为实机 Encaps「Host 折 μ」改法提供 SIM 证据。

## 背景（主控已核对实机代码）
stable encaps `f203_encrypt_l18_l19` 在 `skipNtt` 时：AIC **立刻** `Wait(ST_IP_AIV_DONE=4)`；AIV0 先 `PrefixEmbedMuIntoE2Gm` 再 `TR_AIV_MU_E2`，双 AIV 做 at_jp 后才 `SET(4)`。空 TRACE ⇒ AIV0 未到 μ 标记。TASK-004 已证：缺 SET(4)⇒SIM 挂。

## 允许改动
- 白名单：`ascendc-tests/fix-encrypt-skel-mix-chain-toy/`、`FB-TASK-005.md`、STATUS/INDEX
- 禁止：改 stable Encaps、frozen、真 SHAKE、SyncAll@Wait、commit/push、子 agent、并行 SIM

## 步骤
1. `SKEL_SKIPNTT=1` 下增加：
   - `SKEL_HOST_MU=1`（默认测）：Host 在 launch 前完成「μ 折入」占位（写 GM 标记即可）；**设备 AIV 跳过** μ-stub，尽快双 AIV `SET(4)`（可保留极短 stub）。
   - `SKEL_HOST_MU=0`：设备 AIV0 做 **μ-stub**（小块 DataCopy GM↔UB 模仿 PrefixEmbedMu 形态，勿真编解码），再双 AIV `SET(4)`。
2. 串行 SIM：
   ```bash
   cd /workspace/ascendc-tests/fix-encrypt-skel-mix-chain-toy
   SKEL_SKIPNTT=1 SKEL_HOST_MU=1 SKEL_OMIT_SET4=0 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
   SKEL_SKIPNTT=1 SKEL_HOST_MU=0 SKEL_OMIT_SET4=0 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
   # 回归：缺 SET 仍应挂
   KERNEL_COMPUTE_BUDGET_SEC=60 SKEL_SKIPNTT=1 SKEL_HOST_MU=1 SKEL_OMIT_SET4=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
   ```
3. 期望：HOST_MU=0/1 均绿；OMIT 仍 124。在 FEEDBACK 写清：Host 折 μ **不破坏** skipNtt 握手；并简述若迁到 stable encaps 应改 Host 哪段、设备删哪段（**本单不改 stable**）。
4. 写 `graph_tests/_outbox/FB-TASK-005.md`。

## 反馈要求
FEEDBACK 模板 + 图谱影响表。禁止 commit/push。
