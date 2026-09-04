# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：新 Cloud / 本地 Agent 的**唯一短真相**；本文件优先于长对话历史。  
> **入口**：[`AGENTS.md`](AGENTS.md) → **本文件** → Rule / Skill。  
> **最后刷新**：2026-09-04（Cloud SIM 已绿 Hostμ/TRACE；**等用户 NPU 短报**）

---

## ★ 给新 Agent 的 60 秒上手

1. 分支：**`cursor/kem-2launch-sticky-1534`**。  
2. 主线卡死 = **PKE** Encrypt / Decrypt（非 KEM）。  
3. 请用户上机：**禁止**要长日志；只收**一行式短报**。

### 实机回报格式（强制极短）

```text
PKE-Enc: FORCE绿; 不FORCE第N轮挂; 末行=launch2 l18; 卡1
PKE-Dec: FORCE+TRACE挂; stages=0/13; 末行=prod input; 卡1
```

### 2026-09-04 短报 → Cloud 已做

| 项 | 状态 |
|----|------|
| 用户短报 | Enc 第7轮 l18；Dec FORCE 第1轮 gen_data 后 |
| **PKE Enc Hostμ** | Cloud **CPU+SIM 绿**（默认 `F203_HOST_FOLD_MU`） |
| **PKE Dec TRACE** | Cloud **CPU+SIM 绿**（`F203_DECRYPT_TRACE=1`；SIM 为 sync 后 dump） |

### 待办快照

| 项 | 说明 |
|----|------|
| **P0** | 请用户 NPU：**Enc FORCE 多轮**（Hostμ 已默认）一行短报 |
| **P1** | 请用户 NPU：**Dec FORCE + `F203_DECRYPT_TRACE=1`** 一行短报（可含 stages=） |
| **P2** | KEM 不挡 PKE |

---

## ★ 图谱

| 线 | 用例 | 图谱 | 下一刀 |
|----|------|------|--------|
| PKE Encrypt | `stable-…-pke-encrypt-k4` | `rg-kem-encrypt-hang.yaml` | `D-await-npu-pke-hostmu` |
| PKE Decrypt | `stable-…-pke-decrypt-k4` | `rg-kem-decrypt-hang.yaml` | `D-await-npu-decrypt-trace` |
