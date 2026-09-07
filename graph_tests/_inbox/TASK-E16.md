# TASK E16

## 元数据
- task_id: E16
- issued_at: 2026-09-06T17:56:00Z
- deadline_min: 40
- max_retries: 1
- silent_hang_min: 10
- abort_on: [timeout, no_progress, scope_breach, sim_hang]
- graph: docs/rg-encrypt-npu-hangfree.yaml
- related_nodes: [D-exp-e16, D-defer-correctness-until-hang, J-sim-not-sticky, D-trace-digits]
- write_graph: no
- rules: graph_tests/SUBAGENT_RULES.md

## 背景（主控定调）
- **目标**：服务 `Q-root-cause`（实机粘性挂取证），不是正确性。
- **用户锁**：ByteDecode / 权威交叉 / KAT **不做**。
- **事实**：SIM 不能复现粘性挂 → 需要整份 **NPU_SUITE + 三位数字 TRACE**。

## 目标
新建 `graph_tests/npu_suite/`：把已绿、与挂因相关的 toys 收成**可实机整份跑**的数字 TRACE 套件 + 手抄回报模板。

## 白名单
- ONLY `graph_tests/npu_suite/**`
- 可**只读参考**（禁止修改）：
  - `graph_tests/toys/toy-e01-2launch-set4-trace-repeat/`
  - `graph_tests/toys/toy-e13-encrypt-shaped-glue/`
  - `graph_tests/toys/toy-e15-samplentt-a-full-2x2/`
  （若本地目录名有细微差异，以 `graph_tests/toys/` 实名为准）
- 套件内可用 **薄包装/脚本/对照表/拷贝入口**；若必须拷贝源码进 suite，在 ORIGIN 标明来源且**不得改原 toy**
- 禁止改 Encrypt / 原探针 / E01–E15 源目录 / 图谱 yaml

## 必须交付
1. **SUITE.md / RUNBOOK.md**：用户如何在 NPU 上按序跑；超时/挂起如何停；如何只回报三位数字
2. **TRACE_MASTER.md**：套件级三位号总表（对齐知识库 §6；各 case 号段不冲突或有前缀约定）
3. **cases**：至少 3 档挂因阶梯（建议）
   - C0：e01 握手壳（SET4）
   - C1：e13 形态粘合
   - C2：e15 Â2×2 + 粘合
4. **回报模板**：`REPORT_TEMPLATE.md`（用户只填数字序列 / 卡在哪一号）
5. **smoke**：在本环境对每档至少 `SIM_DIRECT=1` **1 轮**能跑通（证明包装未坏）；**不要**做 golden/权威交叉
6. `_outbox/FEEDBACK-E16.md`（**support** `D-exp-e16`）

## 明确不做
- ByteDecode / t̂
- liboqs / KAT / 正确性门禁
- 复测 retracted 充分条件
- 抄 Encrypt
- 并行 SIM；commit/push

## 验收
- [ ] `npu_suite/` 可按 RUNBOOK 理解并准备上机
- [ ] TRACE 总表 + 三档 case 齐
- [ ] 每档 SIM 1 轮 smoke 不挂（日志路径写入 FEEDBACK）
- [ ] ≤40min；超时 ABORT 写清已完成项
