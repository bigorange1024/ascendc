# TASK-006

## 元数据
- task_id: TASK-006
- issued_at: 2026-09-03T10:41Z
- deadline_min: 50
- max_retries: 1
- silent_hang_min: 12
- abort_on: [timeout, no_progress, scope_breach, sim_hang]
- graph: docs/rg-kem-encrypt-hang.yaml
- related_nodes: [D-next-stable-host-mu, F-host-mu-ok-sim, J-empty-trace-aic-wait4, J-real-aiv0-before-mu-mark, D-goal-npu]
- hypothesis_under_test: [D-next-stable-host-mu, J-real-aiv0-before-mu-mark]
- write_graph: no
- concurrency: solo

## 目标（一句话）
在 **stable Encaps** 上将 `e₂+=μ` **折到 Host**（l18 launch 前），设备 `skipNtt` 路径**跳过** `PrefixEmbedMuIntoE2Gm`；**仅 SIM** 对拍绿，作为实机消粘候选改法。

## 图谱依据
- 已证：缺 SET(4)⇒挂；skipNtt AIC 入口 Wait(4)；空 TRACE⇒AIV0 未到 μ 标记。
- toy：Host 折 μ 不破坏握手（FB-005）。

## 允许改动范围
- 白名单（仅 1024 Encaps 这一棵）：
  - `examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-encaps-k4/`
  - 若该树通过 `#include` 引用同目录/同算子头，可改其直接依赖的 encaps 内文件
  - `graph_tests/_outbox/FB-TASK-006.md`
  - 可选：`graph_tests/INDEX.md` 一行
- 禁止：Decaps、其它 stable、incubating、frozen、toy 大改、commit/push、子 agent、并行 SIM、SyncAll@Wait、滥 launch

## 步骤
1. 读清默认 2-launch：`prep_ntt` → `l18(ySrc=nullptr)`；确认 Host 在 l18 前已有 `m` 与 `e2` 缓冲。
2. Host：在 launch l18 **之前**完成与设备 `PrefixEmbedMuIntoE2Gm` **I/O 等价**的 `e₂+=μ (mod q)`（可用已有 host/C 工具函数；须中文注释标明对齐设备语义）。
3. 设备：`skipNtt==true` 时**跳过** `PrefixEmbedMuIntoE2Gm` 与对应 `TR_AIV_MU_E2`；`skipNtt==false`（fused 对照）保持原行为或显式文档化。
4. 可用 env 开关（推荐）：`F203_HOST_FOLD_MU=1` 默认开（本线生产默认=开）；`=0` 回退旧设备 μ（调试）。
5. 验收（**仅 SIM**）：
   ```bash
   cd /workspace/examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-encaps-k4
   SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
   # 若有开关，再对照：
   F203_HOST_FOLD_MU=0 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
   ```
6. 失败：只允许 1 次针对性修复；仍红 → FEEDBACK FAIL，停止。
7. 写 `FB-TASK-006.md`：写清改了哪些符号、SIM 日志尾、是否建议用户上 NPU。

## 反馈要求
FEEDBACK 模板 + 图谱影响表。禁止 commit/push。
