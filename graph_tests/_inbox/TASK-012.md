# TASK-012

## 元数据
- task_id: TASK-012
- issued_at: 2026-09-03T17:55Z
- deadline_min: 30
- max_retries: 1
- silent_hang_min: 10
- abort_on: [timeout, no_progress, scope_breach, sim_hang]
- graph: docs/rg-kem-decrypt-hang.yaml
- related_nodes: [D-next-dirty-softsync, J-dirty-softsync-hang-vs-race, F-host-zeros-softsync, J-sim-empty-gm-spin-not-hang]
- hypothesis_under_test: [J-dirty-softsync-hang-vs-race]
- write_graph: no
- concurrency: solo

## 目标（一句话）
测 Host softSync 脏值：预填 `1` 更像误放行（SIM 仍绿）；预填 `0` 像清零（仍绿）。**不要**期望/硬凑 124；本单沉积「脏哨兵≠SIM hang」。

## 允许改动
- 白名单：`ascendc-tests/fix-decrypt-skel-mix-chain-toy/`（`main.cpp`/`run.sh`/`STATUS`）、`graph_tests/_outbox/FB-TASK-012.md`、可选 INDEX
- 禁止：改设备 SoftSync 空 while 来硬凑 hang；stable/frozen；改 yaml；commit/push；与其它 OMIT_* 故障开关叠开

## 步骤
1. Host 侧加 env（**运行时即可**，不必 cmake 宏）：
   - `SKEL_SOFTSYNC_PREFILL=0`（默认）：现有清零行为
   - `SKEL_SOFTSYNC_PREFILL=1`：launch 前把 softSync int32[2] **都写成 1**（脏非零）
2. 设备侧**不要**改。注释写明：测 `J-dirty-softsync-hang-vs-race`；预填 1 使 AIV1 跳过自旋（误放行），不是 hang 注入。
3. 串行 SIM（两档都预期 **绿**）：
   ```bash
   cd /workspace/ascendc-tests/fix-decrypt-skel-mix-chain-toy
   SKEL_SOFTSYNC_PREFILL=0 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
   SKEL_SOFTSYNC_PREFILL=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
   ```
4. 判读：两档都绿 → **support**「脏非零更像误放行而非 hang」（weaken 把脏 GM 当 hang 充分条件）。任一挂 → 记录，可能实现错误。
5. STATUS + FEEDBACK。日志：`/opt/cursor/artifacts/decrypt-skel-toy-dirty0.log` / `decrypt-skel-toy-dirty1.log`。

禁止 commit/push。
