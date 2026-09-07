# TASK-002

## 元数据
- task_id: TASK-002
- issued_at: 2026-09-03T10:06Z
- deadline_min: 40
- max_retries: 1
- silent_hang_min: 12
- abort_on: [timeout, no_progress, scope_breach, sim_hang]
- graph: docs/rg-kem-encrypt-hang.yaml
- related_nodes: [F-skel-toy-sim-pass, J-hang-needs-extra-factor, J-empty-trace-aic-wait4, J-common-mix-flag13, Q-toy-repro, D-next-stress-skel, D-forbid-syncall-while-wait, F-gate-4-8]
- hypothesis_under_test: [J-hang-needs-extra-factor, J-empty-trace-aic-wait4, Q-toy-repro]
- write_graph: no
- concurrency: solo

## 目标（一句话）
在已绿的 `fix-encrypt-skel-mix-chain-toy` 上**增量加压**：插入 **GATE 4/8**（对齐 l18 at_jp 后握手形态），SIM 观察是否仍正常结束或逼近挂死；记录对假说的支持/削弱。

## 允许改动范围
- 路径白名单：
  - `ascendc-tests/fix-encrypt-skel-mix-chain-toy/`（仅本探针）
  - `graph_tests/_outbox/FB-TASK-002.md`
  - 可选更新本探针 `STATUS.md`、`graph_tests/INDEX.md` 一行
- 禁止：stable Encaps、frozen 抄码、真 SHAKE、SyncAll@AIC-Wait、自造 SoftSync、滥 Host launch、碎写 GM、commit/push、再派子 agent、并行 SIM

## 必读
1. `graph_tests/_inbox/TASK-002.md`（本文件）、`CHARTER.md`、`SUBAGENT_RULES.md`
2. `graph_tests/_outbox/FB-TASK-001.md`（基线）
3. 现有探针源码（尤其 CrossCore 1/3 段）
4. API 查阅索引中 CrossCore / GATE 相关记录

### 图谱摘录
| id | status | 要点 |
|----|--------|------|
| F-skel-toy-sim-pass | verified | 轻量 1/3 双轮 SIM 已绿 |
| J-flag13-alone-sufficient-sim | retracted | 仅 1/3 不足以 SIM 挂 |
| J-hang-needs-extra-factor | unverified | 需附加因子；本单测 GATE |
| J-empty-trace-aic-wait4 | unverified | 空 TRACE↔WAIT(4)/GATE |
| D-next-stress-skel | active | 加压 GATE / 放大 MMAD |

## 步骤
1. **保留** TASK-001 基线可切换（推荐：编译宏或 env `SKEL_GATE=0/1`，默认 `1` 测加压；`0` 应仍接近基线绿）。
2. 在 stub_inner 与 INTT-like 之间（或对齐 l18：NTT-like 之后、INTT-like 之前）插入 **GATE CrossCore flag 4↔8** 最小握手：
   - 双 AIV 参与 SET/WAIT 对称（参考 Encrypt/Decrypt GATE 形态的**最小子集**，勿搬全量 at_jp）
   - AIC 侧按既有合法模式 Wait/Set；**禁止** AIC 仍 Wait 时 SyncAll
3. 仍 1 MIX launch；stub 哈希；verify 仍只查 magic（可改次要字节标记 GATE 开时不同，但须文档化）。
4. 验收仅 SIM：
   ```bash
   cd /workspace/ascendc-tests/fix-encrypt-skel-mix-chain-toy
   SKEL_GATE=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
   # 对照（若实现了开关）：
   SKEL_GATE=0 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
   ```
5. 若 `SKEL_GATE=1` **挂死/超时 124**：不要无限重试；保存日志，FEEDBACK 记 `support` 对 J-hang-needs-extra-factor / J-empty-trace-aic-wait4，然后停止。
6. 若仍绿：FEEDBACK 记 `weaken` 或 `n/a`（GATE  alone 在 stub 下不足），可**可选**做一档更大 MMAD（仍 max_retries 内一次），否则交卷。
7. 写 `graph_tests/_outbox/FB-TASK-002.md`。

## 反馈要求
严格 FEEDBACK 模板；必须填图谱影响表。禁止 commit/push。
