# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：新 Cloud / 本地 Agent 的**唯一短真相**；本文件优先于长对话历史。  
> **入口**：[`AGENTS.md`](AGENTS.md) → **本文件** → Rule / Skill。  
> **最后刷新**：2026-09-04（分支已推；**用户将代跑 Decrypt NPU**）

---

## ★ 给新 Agent 的 60 秒上手

1. 分支：**`cursor/kem-2launch-sticky-1534`**（已推至 `793f141`）。  
2. Decrypt hang：**T0–T4 SIM 齐**；用户按下方清单上机。  
3. 三张图分线；同时仅一个 subagent。

### 待办快照

| 项 | 说明 |
|----|------|
| **P0** | **收用户 NPU 日志**：PKE Decrypt fused 是否仍挂；贴最后 30～80 行 |
| **P1** | 可选：toy `fix-decrypt-skel-mix-chain-toy` 合法/OMIT_SET4 对照 |
| **P1** | Encaps Hostμ / Decaps K=131 正交 |

---

## ★ Decrypt hang 沉积（SIM）

| 实验 | 结果 |
|------|------|
| 合法握手 | 绿，magic `SKELDEC1` |
| 两轮都不 SET(4) / 仅第二轮不 SET(4) | **124** |
| 空 while / 脏 softSync | SIM **仍绿** |
| 双 Cube 充分 hang | **retracted** |

图谱：`docs/rg-kem-decrypt-hang.yaml` · toy：`ascendc-tests/fix-decrypt-skel-mix-chain-toy/`

---

## ★ 用户 NPU（本轮授权）

见当日对话「实机怎么测」清单：PKE `stable-fips203-mlkem-pke-decrypt-k4` 优先；`DECRYPT_FORCE_REBUILD=1` 一次后连跑勿每轮 FORCE；卡号 stable=**1** / tests=**3**。
