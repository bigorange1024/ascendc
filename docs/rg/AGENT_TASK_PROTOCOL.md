# 主控 ↔ Subagent 协议（Encrypt 卡死排查）

图谱真理源（按 TASK 元数据 `graph:` 字段）：
- Encaps hang：[`../rg-kem-encrypt-hang.yaml`](../rg-kem-encrypt-hang.yaml)
- **Decrypt hang**：[`../rg-kem-decrypt-hang.yaml`](../rg-kem-decrypt-hang.yaml)
- Decaps K：[`../rg-kem-decrypt-k131.yaml`](../rg-kem-decrypt-k131.yaml)  
工具：`thirdparty/reasoning-graph-skill-master/scripts/`  
实验根：`graph_tests/` · 探针根：`ascendc-tests/`  
**总章程**：[`../../graph_tests/CHARTER.md`](../../graph_tests/CHARTER.md)  
**单工/时限**：[`../../graph_tests/SUBAGENT_RULES.md`](../../graph_tests/SUBAGENT_RULES.md)  
**Git**：不得自主推送。

---

## 目标分层

| 层级 | 内容 | 节点 |
|------|------|------|
| **最终** | NPU 实机 Encrypt 不再卡死在 `l18_l19`（及同类 MIX 挂点），且正确性通过 | `D-goal-npu` |
| **当前** | 轻量 toy 模仿 Encrypt 任务流；快 SIM；打通主控–图谱–subagent–实验闭环；沉淀可推翻的正确知识 | `D-near-toy` / `D-next-build-toy` |

主控只下发**当前**目标下的原子任务；不把「修完 stable Encaps」塞进单次 subagent 工单。

---

## 图谱入库门禁（主控刷新时强制）

| 收 | 不收 |
|----|------|
| 能直接服务 **Encrypt 卡死 debug** 的事实/推论/决策/问题 | 无关流程、闲聊、目录导游、与本 bug 无关的正确性琐事 |
| **失败实验 / 已证伪假说 / 已回退方案**（`retracted` / `inactive` + 证据） | 只堆「对拍绿」成功路径、把失败写进口头不写进图 |
| 合法同步与可复现观测 | 把「逐步外搬 GM」「滥增 Host launch」「标量碎写」等**严重伤效率/性能**的 correctness 捷径标成 **active 推荐解** |

反模式若曾试过：必须入库为 **inactive/retracted 负面知识**（防重踩），**禁止**当成推荐沉淀。节点见 `D-admit-*`、`D-reject-correctness-antipattern`、`D-antipattern-*`。

**验证门禁（本排查线）**：最终看 **NPU 实机**；迭代以 **`SIM_DIRECT=1` sim** 为准。  
**不跑 CPU 作门禁、不沉淀 CPU 孪生经验**（`D-verify-sim-for-npu`）。

反馈模板里 **effect=refute/weaken** 与失败日志，权重不低于 PASS。

---

## 下发模板（主控 → subagent）

复制为一次工单（建议文件名 `graph_tests/_inbox/TASK-<YYYYMMDD>-<序号>.md`）：

```markdown
# TASK <id>

## 元数据
- task_id:
- issued_at:
- deadline_min:          # 必填；到点无 FEEDBACK → 主控 ABORT
- max_retries: 1
- silent_hang_min: 10
- graph: docs/rg-kem-encrypt-hang.yaml
- related_nodes: [ ]   # 必读
- hypothesis_under_test: []  # 本次要支持/削弱/证伪的 J*/Q*
- write_graph: no | yes(only: id1,id2)  # 默认 no
- concurrency: solo    # 同时仅本 TASK；禁止再派子 agent / 并行 SIM

## 目标（一句话）
<只写本工单完成标准>

## 允许改动范围
- 路径白名单：
- 禁止：改 stable Encaps 全量、改 .cursor/rules|skills、从 frozen 抄码、未授权 commit/push、并行 SIM、无限返工超过 max_retries

## 必读材料（按序）
1. 图谱节点：用下面摘录；需要时 `python3 thirdparty/reasoning-graph-skill-master/scripts/rg_query.py --yaml docs/rg-kem-encrypt-hang.yaml --show <id>`
2. …

### 图谱摘录（主控粘贴）
| id | kind | status | statement |
|----|------|--------|-----------|
| … | … | … | … |

## 步骤（按序执行，勿跳）
1.
2.

## 验收命令（原样跑，贴完整尾部日志）
```bash
…
```

## 反馈要求
按 FEEDBACK 模板写到 `graph_tests/_outbox/FB-<task_id>.md`。
未完成也要交反馈（写阻塞点）。
```

---

## 反馈模板（subagent → 主控）

```markdown
# FEEDBACK <task_id>

## 结果摘要
- outcome: PASS | FAIL | BLOCKED | PARTIAL
- one_liner:

## 对图谱的影响（主控据此刷新；subagent 默认不改 yaml）
| node_id | effect | evidence_path |
|---------|--------|---------------|
| J-… | support / weaken / refute / n/a | path:line 或 log |

> **失败优先**：若实验失败或证伪假说，上表必须写满 refute/weaken，并附日志；不要只报「没绿」而不指向节点。

## 实际改动
- files:
- 未改（说明）：

## 命令与关键日志
```bash
# 命令（本线默认只跑 SIM，例：SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4）
# 退出码
# 最后 30～80 行或关键片段
```

## SIM 墙钟（必填若跑了 sim）
- sim_sec:
- vs_full_encaps: faster | unknown
# 不要求、不记录 cpu_sec

## 意外发现（新事实候选，勿写成长叙事）
- 

## 建议下一刀（可选，主控可不采纳）
- 
```

---

## 主控刷新图谱检查单

每次收到反馈后：

1. `rg_validate.py` 前先改 yaml（新 fact / status / retracted）
2. 若某 J 被 refute：`status: retracted` + `retracted_by`，再 `--dependents-of` 扫下游
3. `rg_validate.py` + `rg_audit.py`
4. 决定下一份 TASK（或暂停问用户）

---

## 查询速查

```bash
RG=docs/rg-kem-encrypt-hang.yaml
S=thirdparty/reasoning-graph-skill-master/scripts
python3 $S/rg_query.py --yaml $RG --stats
python3 $S/rg_query.py --yaml $RG --status unverified
python3 $S/rg_query.py --yaml $RG --dependents-of J-common-mix-flag13
python3 $S/rg_query.py --yaml $RG --show F-trace-empty-0-16
```
