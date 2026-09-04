# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：新 Cloud / 本地 Agent 的**唯一短真相**；本文件优先于长对话历史。  
> **入口**：[`AGENTS.md`](AGENTS.md) → **本文件** → Rule / Skill。  
> **最后刷新**：2026-09-04（短报入库；**下一刀** PKE Enc Hostμ + Dec TRACE）

---

## ★ 给新 Agent 的 60 秒上手

1. 分支：**`cursor/kem-2launch-sticky-1534`**。  
2. 主线卡死 = **PKE** Encrypt / Decrypt（非 KEM）。  
3. 请用户上机时：**禁止**要长日志；只收**一行式短报**。

### 实机回报格式（强制极短）

```text
PKE-Enc: FORCE绿; 不FORCE第7轮挂; 末行=launch2 l18; 卡1
PKE-Dec: FORCE挂; 末行=prod input; 卡1
```

### 2026-09-04 短报（已入库）

| 用例 | 结果 |
|------|------|
| PKE Encrypt | 第 **7** 轮挂；末行仍 `launch 2 … l18_l19` |
| PKE Decrypt | **FORCE 第1轮**挂；末行仍 `prod input … _gen_fixture` |

纪要：`qa/2026-09/2026-09-04-PKE-EncryptDecrypt实机短报老挂点.md`

### 待办快照

| 项 | 说明 |
|----|------|
| **P0** | **PKE Encrypt**：迁 Host 折 μ（`D-next-pke-encrypt-hostmu`）→ Cloud SIM 绿 → 再请短报 |
| **P1** | **PKE Decrypt**：fused/toy 加 TRACE 段标记（`D-next-device-trace-marks`）→ SIM → 短报 |
| **P2** | KEM 不挡 PKE；clean Hostμ 树并行对照 |

---

## ★ 图谱

| 线 | 用例 | 图谱 |
|----|------|------|
| PKE Encrypt | `stable-…-pke-encrypt-k4` | `rg-kem-encrypt-hang.yaml` |
| PKE Decrypt | `stable-…-pke-decrypt-k4` | `rg-kem-decrypt-hang.yaml` |
| Decaps K≠0 | （正交，非挂） | `rg-kem-decrypt-k131.yaml` |

章程：`graph_tests/CHARTER.md` · 协议：`docs/rg/AGENT_TASK_PROTOCOL.md`
