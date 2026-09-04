# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：新 Cloud / 本地 Agent 的**唯一短真相**；本文件优先于长对话历史。  
> **入口**：[`AGENTS.md`](AGENTS.md) → **本文件** → Rule / Skill。  
> **最后刷新**：2026-09-04（**PKE Encrypt→Decrypt**；实机回报须**极短**，禁要长日志）

---

## ★ 给新 Agent 的 60 秒上手

1. 分支：**`cursor/kem-2launch-sticky-1534`**。  
2. 主线卡死 = **PKE** Encrypt / Decrypt（非 KEM）。  
3. 请用户上机时：**禁止**要「最后 30～80 行」；用户须手敲，只收**一行式短报**。

### 实机回报格式（强制极短）

每用例最多类似一行（可改数字）：

```text
PKE-Enc: FORCE绿; 不FORCE第3轮挂; 末行=launch 2 l18; 卡1
PKE-Dec: FORCE挂; 末行=prod input; 卡1
```

允许字段：绿/挂/红；第几轮；末行**关键词 ≤15 字**；卡号。  
禁止：整段日志、几十行粘贴、长路径、完整栈。

### 待办快照

| 项 | 说明 |
|----|------|
| **P0** | 收用户 **一行式** NPU 短报（Encrypt 先、Decrypt 后） |
| **P1** | 据短报刷新图谱；不够再问**一个**关键词 |
| **P2** | KEM 不挡 PKE |

---

## ★ 图谱

| 线 | 用例 | 图谱 |
|----|------|------|
| PKE Encrypt | `stable-…-pke-encrypt-k4` | `rg-kem-encrypt-hang.yaml`（验收以 PKE 为准） |
| PKE Decrypt | `stable-…-pke-decrypt-k4` | `rg-kem-decrypt-hang.yaml` |
