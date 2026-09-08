# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：新 Cloud / 本地 Agent 的**唯一短真相**；本文件优先于长对话历史。  
> **入口**：[`AGENTS.md`](AGENTS.md) → **本文件** → Rule / Skill。  
> **最后刷新**：2026-09-08（Encrypt/Encaps cannbot：**Q-ULT answered**；T24×30 不挂；已推送收口版）

---

## ★ 给新 Agent 的 60 秒上手

1. 分支：当前多为 `chore/thirdparty-add-cannbot-skills`（**勿擅自开分支**）。  
2. **真相**：设备 **Encrypt + Encaps** 已在 NPU 权威绿；重写目的=**不卡死**（KB §B2）。  
3. 终态用例：`graph-tests/enc_related/RB-T22` / `T23` / `T24`。  
4. KB/DAG：[`Encrypt-cannbot-rebuild-kb.md`](docs/notes/Encrypt-cannbot-rebuild-kb.md) · [`rg-encrypt-cannbot-rebuild.yaml`](docs/rg-encrypt-cannbot-rebuild.yaml)。  
5. 压测：T24 ×30 `ok=30`（`/mnt/workspace/encrypt-rebuild-hang-stress.log`）。

### 刚关闭

| 项 | 说明 |
|----|------|
| Q-ULT | answered |
| T22–T24 | CPU+NPU 双绿；T24×30 不挂 |
| G1–G5 | 清单已关 |

### 待办快照

| 项 | 说明 |
|----|------|
| **可选** | 设备 Decaps |
| **NPU** | T06 sticky 保活（可停） |
| **commit** | 收口版已按用户指令推送 |

---

## ★ 接手清单

| 优先 | 项 | 注意 |
|------|----|------|
| P0 | 读 KB §B2 + QUEUE | 勿破坏双 launch / 1·3+4 / BLOCK_DIM=1 |
| P1 | 可选设备 Decaps | 禁抄旧 decaps |
| P2 | 无授权不 commit/push | |

**别做**：fork 旧 Encrypt；刷已绿刀空烧；未授权开分支。
