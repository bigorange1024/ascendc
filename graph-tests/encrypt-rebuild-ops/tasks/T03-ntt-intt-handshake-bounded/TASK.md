# T03 — NTT → GATE → INTT 握手（有界体量；禁 5/7）

| 字段 | 值 |
|------|-----|
| 状态 | **blocked_dep**（须 T02 PASS） |
| DAG | `D-EXP-T03` |
| 代码目录 | `graph-tests/toys/RB-T03-ntt-gate-intt-bounded/` |
| 运营目录 | `…/tasks/T03-ntt-intt-handshake-bounded/` |
| 墙钟 | ≤ 50 min |

继承 COMMON。

## 目标

单 launch MIX：**NTT 握手 1/3** + **GATE Wait(4)** + **INTT 握手仍用 1/3 复用或独立非 5/7 flag**（在 TASK 实现说明里写清 flag 表；**永禁 5/7**）。  
体量：有界真计算（轻 Vec/MAC 或极短 Cube），禁止「空转加码」冒充压力。  
验收：**SIM 不挂**。

## 非目标

正确 Â∘ŷ；liboqs；抄旧 fuse 核。

## 必读

- KB §A2（poly-batch / Gather 禁令，若动到 NTT 真积木）  
- `MLKEM-NTT-向量与标量实现指南.md` §Gather 范围  
- T01/T02 失败教训

## 验收

- CPU+SIM；完整 `trace_map.md` + flag 表  
- sync_audit 干净  
- FEEDBACK 含「若挂，卡在哪段 TRACE」假设

## 依赖

T02 = PASS。
