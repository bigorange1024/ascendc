# Subagent 单工与时间规则

> 主控必须执行；每份 TASK 须引用或粘贴本摘要。  
> 依据：用户 2026-09-03 — 同时只能一个 subagent；不得傻等卡死/死循环/无限返工。

---

## 1. 并发

| 规则 | 说明 |
|------|------|
| **同时最多 1 个干活 subagent** | 含写码、编译、SIM |
| **禁止并行 SIM** | 不得同时两路 `run.sh -r sim` |
| **单 TASK 生命周期** | 下发 → 执行 → FEEDBACK 或 ABORT → 主控关门 → 才可下发下一单 |

---

## 2. 每份 TASK 必填时限字段

在 TASK 元数据中强制：

```text
deadline_min: <整数，建议 15–45；toy 骨架首建可至 60>
max_retries: <整数，默认 1>
silent_hang_min: <整数，默认 10；SIM/编译无新日志超过即疑似挂死>
abort_on: [timeout, no_progress, scope_breach, sim_hang]
```

主控时钟以墙钟为准；到点未交 FEEDBACK → **ABORT**，不等「再试一次」。

---

## 3. Subagent 行为契约（写进 TASK）

1. **只改白名单路径**；越权立即停并 FEEDBACK `BLOCKED`。  
2. **先出最小可跑版本**，再增强；禁止一上来复刻全量 Encaps/重哈希。  
3. 本线验收：**SIM only**；不要求 CPU。  
4. SIM 须带用例自己的 `KERNEL_COMPUTE_BUDGET_SEC`；触发 124 → 记失败，**不要死磕重跑同一命令超过 max_retries**。  
5. 卡住超过 `silent_hang_min` 无进展 → 停、留日志路径、交 FEEDBACK。  
6. **失败必须总结**：FEEDBACK 里 `effect=refute/weaken` 指向节点；禁止只写「没跑通」。  
7. **默认不改图谱 yaml**；不 commit/push。  
8. 不得自行再派子 subagent 做 SIM/写码（防止隐式并行）。

---

## 4. 主控止损动作

| 触发 | 主控做什么 |
|------|------------|
| 超时 / 静默挂死 | 宣告 ABORT；保留部分产物；图谱记失败 decision/inference |
| 同错返工超限 | ABORT；改假说或缩小 TASK，**不**让同一 subagent 无限续跑 |
| scope 越权 | 立即停；回滚或丢弃越权改动（按情况） |
| FEEDBACK 缺图谱影响表 | 退回补写一次；仍缺则主控代填后关门 |

原则：**时间预算用完就换刀或升问题到用户，不空转。**

---

## 5. 推荐首单时限（可调）

| TASK 类型 | deadline_min | max_retries |
|-----------|--------------|-------------|
| 建 toy 骨架 + 一次 SIM 冒烟 | 45–60 | 1 |
| 单假说 graph_tests 小实验 | 20–30 | 1 |
| 仅改同步/加 TRACE 再 SIM | 25–40 | 1 |
