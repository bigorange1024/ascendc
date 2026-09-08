# T09 — Encrypt 外形 2-launch 骨架（SIM 不挂）

| 字段 | 值 |
|------|-----|
| 状态 | **blocked_dep**（T06 + T07 + T08） |
| DAG | `D-EXP-T09` · 拼装里程碑 |
| 代码目录 | `graph-tests/enc_related/RB-T09-encrypt-shaped-2launch/` |
| 运营目录 | `…/tasks/T09-encrypt-shaped-2launch/` |
| 墙钟 | ≤ 60 min |

继承 COMMON。

## 目标

**外形**双 launch（非抄旧 Encrypt）：

1. Launch1：prep（可真可半桩，优先接 T07 输出）  
2. Launch2：compute 壳 = T03 级握手 + T08 级有界体 + 可选接 T06 pack  

主验收：**SIM 双 launch 不挂**。数值正确性：**尽力对拍**，但本刀允许 `PASS_SYNC`（不挂+审计绿）与 `PASS_IO`（c 对齐）分栏写 STATUS；不得用 IO 绿掩盖 sync 红。

## 必读

- 全部前驱 FEEDBACK · KB §B3 · §X5  
- cannbot sync-audit + deadlock-triage

## 验收

- CPU + SIM；无 hang  
- sync_audit 干净  
- FEEDBACK 明确 PASS_SYNC / PASS_IO / FAIL  
- 半页「NPU 复现步骤」草稿写入 `logs/npu-repro-draft.md`（供 N02）

## 依赖

T06 + T07 + T08 = PASS。
