# T02 — GATE 时序：AIC 先 Wait(4) + 轻体

| 字段 | 值 |
|------|-----|
| 状态 | **ready**（T01 PASS） |
| DAG | `D-EXP-T02` |
| 代码目录 | `graph-tests/toys/RB-T02-gate-timing-wait4/` |
| 运营目录 | `…/tasks/T02-gate-timing-wait4/` |
| 墙钟 | ≤ 45 min |

继承 COMMON。

## 目标

在新目录验证 **生产 Encrypt 常见 GATE 外形**：AIC 侧 **先** `Wait(4)`（由 AIV `Set(4)` 释放），再极轻计算；NTT 段仍用 **1/3**（若本刀含 NTT 握手）。  
核心：**SIM 不挂**；对照 KB §X1（勿未读 TRACE 改 FSM）。

## 非目标

完整 INTT；μ；密文；抄 `l18_l19`。

## 必读

- T01 `STATUS` + FEEDBACK  
- KB §X1–X2；cannbot deadlock-triage  
- inventory：MIX 行

## 验收

- CPU + SIM 绿；`trace_map.md` 含 flag4 与 1/3（若使用）  
- sync_audit 无红线  
- FEEDBACK 说明：AIC 是否在 AIV Set4 之前进入 Wait4（预期如此，靠 Set 唤醒）

## 依赖

T01 = PASS。
