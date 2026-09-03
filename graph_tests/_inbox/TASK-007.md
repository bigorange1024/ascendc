# TASK-007

## 元数据
- task_id: TASK-007
- issued_at: 2026-09-03T11:10Z
- deadline_min: 55
- max_retries: 1
- silent_hang_min: 12
- abort_on: [timeout, no_progress, scope_breach, sim_hang]
- graph: docs/rg-kem-encrypt-hang.yaml
- related_nodes: [D-clean-rewrite-encrypt, D-next-clean-p0, J-empty-trace-aic-wait4, F-host-mu-ok-sim, D-forbid-syncall-while-wait]
- hypothesis_under_test: [D-next-clean-p0]
- write_graph: no
- concurrency: solo

## 目标（一句话）
按 `graph_tests/ENCRYPT_CLEAN_REWRITE.md` 落地 **PHASE-P0**：新建 `ascendc-tests/fix-encrypt-clean-hostmu-2launch/`——Host 2-launch + **默认 Host μ** + 设备 skipNtt **无 PrefixEmbed**；SIM 通。

## 允许改动
- 白名单：
  - `ascendc-tests/fix-encrypt-clean-hostmu-2launch/`（新建）
  - `ascendc-tests/INDEX.md`（一行）
  - `graph_tests/_outbox/FB-TASK-007.md`、可选 INDEX
- 可参考（只读）：`fix-encrypt-skel-mix-chain-toy/`、stable encaps Host 折 μ 思路
- 禁止：改 stable/Decaps、frozen 抄码、真重哈希、SyncAll@Wait、自造 SoftSync、滥 launch、commit/push、子 agent、并行 SIM

## 步骤
1. 读 `ENCRYPT_CLEAN_REWRITE.md` §1–4（P0）。
2. 新建探针：**结构即约束**（不要靠事后开关才「正确」）：
   - Host launch1：轻量 MIX stub 或最小 Cube（一轮）；**不做设备 μ**
   - Host：折 μ 占位/真实小缓冲（默认始终执行）
   - Host launch2：skipNtt 形态——AIC 入口 `Wait(4)`；AIV **无 PrefixEmbed**，短 stub 后双 AIV `SET(4)`；可保留轻量 GATE/INTT-like
3. verify：magic/标记即可（P0 不对 ML-KEM golden）。
4. SIM only：
   ```bash
   cd /workspace/ascendc-tests/fix-encrypt-clean-hostmu-2launch
   SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
   ```
5. `STATUS.md` 写明与图谱不变量的对应；中文注释。
6. `FB-TASK-007.md`。

## 反馈要求
FEEDBACK 模板；图谱影响表。禁止 commit/push。
