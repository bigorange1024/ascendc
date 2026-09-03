# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：新 Cloud / 本地 Agent 的**唯一短真相**；本文件优先于长对话历史。  
> **入口**：[`AGENTS.md`](AGENTS.md) → **本文件** → Rule / Skill。  
> **最后刷新**：2026-09-03（Decrypt hang **T0–T4 SIM 齐**；下一候选 TRACE）

---

## ★ 给新 Agent 的 60 秒上手

1. 分支：**`cursor/kem-2launch-sticky-1534`**。  
2. Decrypt hang：**CrossCore 缺 SET(4) 机制已沉积**；不是空图谱。  
3. **仍不要**默认请用户上全量 Decrypt NPU（差 TRACE / 用户明确授权）。  
4. 三张图分线；同时仅一个 subagent。

### 待办快照

| 项 | 说明 |
|----|------|
| **P0** | 候选：**Decrypt TRACE 层标记**（toy 或生产 fused，先 SIM）— `D-next-device-trace-marks` |
| **P1** | Encaps Hostμ NPU / Decaps K=131 正交，用户有空再跑 |

---

## ★ Decrypt hang 沉积（有条件完成至 SIM CrossCore 层）

| 实验 | 结果 |
|------|------|
| 合法握手 | 绿，magic `SKELDEC1`，wall≈3.2–4.1s |
| 两轮都不 SET(4) | **124** |
| 仅第二轮不 SET(4) | **124** |
| 空 while 缺 slot0 | SIM **仍绿**（非 hang 代理） |
| Host 预填 softSync=1 | SIM **仍绿**（脏≠hang） |
| 双 Cube 充分 hang | **retracted** |

图谱：`docs/rg-kem-decrypt-hang.yaml` · toy：`ascendc-tests/fix-decrypt-skel-mix-chain-toy/` · 计划：`graph_tests/DECRYPT_HANG_PLAN.md`

实机 SoftSync 空自旋是否挂：`J-npu-softsync-spin-open` 仍 unverified（SIM 证不出）。

---

## ★ 下一刀

优先：给 Decrypt 加可 Host 读出的段标记 TRACE（仿 Encaps），仍 SIM 门禁。  
**不要**把「请用户跑 `pke-decrypt-k4 -r npu`」当默认下一刀。
