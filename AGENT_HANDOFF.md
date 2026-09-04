# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：新 Cloud / 本地 Agent 的**唯一短真相**；本文件优先于长对话历史。  
> **入口**：[`AGENTS.md`](AGENTS.md) → **本文件** → Rule / Skill。  
> **最后刷新**：2026-09-04（**纠偏**：禁零散上机；SIM 穷尽 → 一份 NPU 测试组）

---

## ★ 给新 Agent 的 60 秒上手

1. 分支：**`cursor/kem-2launch-sticky-1534`**。  
2. 主线 = **PKE Encrypt/Decrypt 卡死**；图谱驱动；**SIM 穷尽前不上机**。  
3. **禁止**零散请用户测；借入机独立网，**禁止**假设能 `pull`。  
4. 硬自检：凭图谱**还不能**自信重写 Encrypt 不卡 → 继续 SIM/toy，**不要**请 NPU。

### 实机（仅当 `graph_tests/NPU_SUITE.md` 已写齐）

用户跑**一整份测试组**，回报每档一行；禁长日志、禁「再测一个」。

### 当前真相（图谱）

| 问 | 答 |
|----|----|
| SIM 能复现 NPU 粘性挂吗？ | **还不能**（缺 SET(4)⇒124 是另一类；合法 GATE/双 Cube/Hostμ stub 在 SIM 都绿） |
| Hostμ / TRACE 够消挂吗？ | **未知**；SIM 绿 ≠ 根因已知 |
| 能从头新写 Encrypt 保证不卡？ | **不能**（见下） |

### 待办快照

| 项 | 说明 |
|----|------|
| **P0** | Encrypt：**clean P1+** / 能复现粘性的 SIM 代理；把「不卡死写法」沉进图谱 |
| **P1** | Decrypt：SIM 决策树补全后再写进 **同一份** `NPU_SUITE.md` |
| **P2** | 写出 `graph_tests/NPU_SUITE.md` 后才请用户上机（离线包随套件） |

---

## ★ 图谱

| 线 | 图谱 | 备注 |
|----|------|------|
| Encrypt hang | `rg-kem-encrypt-hang.yaml` | `Q-root-cause` / `Q-toy-repro` 仍 open |
| Decrypt hang | `rg-kem-decrypt-hang.yaml` | CrossCore 缺 SET 已沉；粘性根因未沉 |
| Decaps K | `rg-kem-decrypt-k131.yaml` | 正交，不挡 hang |

章程：`graph_tests/CHARTER.md` §1.6
