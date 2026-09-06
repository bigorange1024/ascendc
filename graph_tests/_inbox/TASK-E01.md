# TASK E01

## 元数据
- task_id: E01
- issued_at: 2026-09-06T07:20:00Z
- deadline_min: 35
- max_retries: 1
- silent_hang_min: 8
- graph: docs/rg-encrypt-npu-hangfree.yaml
- related_nodes: [D-exp-e01, D-layer-handshake, D-set4-invariant, D-trace-digits, D-short-experiments, D-no-repeat-retracted, F-omit-set4-sim124, F-set4-ok-sim]
- hypothesis_under_test: [D-exp-e01]
- write_graph: no
- concurrency: solo

## 目标（一句话）
在**新目录**做出极简 2-launch MIX toy：L2 含 Wait(4)/SET(4)、Host 只打数字 TRACE、同进程连续 8 轮 SIM 全跑完；另一次 OMIT_SET4⇒124。

## 允许改动范围
- 白名单：**仅** `graph_tests/toys/toy-e01-2launch-set4-trace-repeat/`（可新建）
- 可**只读**参考：`ascendc-tests/fix-encrypt-skel-mix-chain-toy/`、`fix-encrypt-clean-hostmu-2launch/`（工程壳/同步模式）
- 禁止：改任何冻结目录、stable Encaps/Encrypt、图谱 yaml、知识库；禁止照抄 Encrypt 业务逻辑；禁止并行 SIM；禁止再测「双 Cube/GATE alone 是否充分挂」

## 必读
1. `graph_tests/SUBAGENT_RULES.md`
2. 图谱节点：`D-exp-e01`、`D-short-experiments`、`D-no-repeat-retracted`
3. 知识库 TRACE §6（数字号）
4. 写 AscendC 前读 `.cursor/skills/ascendc-engineering-notes/SKILL.md`

## 步骤（按序，勿跳）
1. 新建白名单目录；可仿 skel 的 CMake/run.sh **工程壳**，但 kernel **自写极简**（L1：几乎空；L2：AIC Wait(4)+过 Wait 打 TRACE；双 AIV SET(4)+TRACE）。
2. Host：数字 TRACE（至少 100/101/110/111；设备 400/401/402、500/502、510/512）；同进程 **for 8**：launch L1 → sync → launch L2 → sync。
3. `SIM_DIRECT=1`（或该壳等价）跑 SIM，确认 8 轮都完成（见 8×111 或等价计数）。
4. 对照：`OMIT_SET4=1`（或你设的开关）再跑应 **124**/超时挂；恢复默认仍绿。
5. 写 `TRACE.md` + `STATUS.md` + `graph_tests/_outbox/FEEDBACK-E01.md`。

## 验收标准（PASS）
- [ ] 仅白名单路径有改动
- [ ] 默认 SIM：同进程 8 轮 2-launch 全完成
- [ ] OMIT_SET4 对照：SIM 挂/124
- [ ] TRACE 只有数字；有 TRACE.md 对照表
- [ ] FEEDBACK 含 effect 表（support D-exp-e01 / 引用 F-omit-set4）
- [ ] 墙钟 ≤35min；超时则停并 FEEDBACK `TIMEOUT`（已有部分也要交）

## 失败也要交
超时/编不过/SIM 挂非预期 → FEEDBACK `FAIL`/`TIMEOUT`，写清日志路径与对图谱影响；**不要死磕重试超 max_retries**。
