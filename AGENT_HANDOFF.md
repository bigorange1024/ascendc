# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：新 Cloud / 本地 Agent 的**唯一短真相**；本文件优先于长对话历史。  
> **入口**：[`AGENTS.md`](AGENTS.md) → **本文件** → Rule / Skill。  
> **最后刷新**：2026-09-03（Decrypt hang T0–T3 已沉；TASK-012 脏 softSync）

---

## ★ 给新 Agent 的 60 秒上手

1. 分支：**`cursor/kem-2launch-sticky-1534`**。  
2. **焦点 = PKE Decrypt 卡死 toy/SIM**，不是请用户上机。  
3. 三张图分线；同时仅一个 subagent。

### 待办快照

| 项 | 说明 |
|----|------|
| **P0** | **TASK-012**：Host softSync 预填 0/1（脏哨兵≠hang） |
| **P1** | CrossCore 缺 SET(4) 已双档 124；勿再空 while 硬凑 |
| **P1** | Encaps Hostμ / Decaps K=131 正交 |

---

## ★ 当前真相（Decrypt hang 沉积）

| 实验 | 结果 |
|------|------|
| 合法握手 | SIM 绿，magic `SKELDEC1`，wall≈3.2–3.9s |
| 两轮都不 SET(4) | **124** |
| 仅第二轮不 SET(4) | **124**（第一轮 Cube 能跑完） |
| AIV0 不写 slot0（空 while） | SIM **仍绿** — 空 while **不是** SIM hang 代理 |
| 双 Cube 充分 hang | **retracted**（借入 Encaps） |

图谱：`docs/rg-kem-decrypt-hang.yaml` · toy：`ascendc-tests/fix-decrypt-skel-mix-chain-toy/`

---

## ★ 下一刀（Cloud，不是 NPU）

```bash
cd ascendc-tests/fix-decrypt-skel-mix-chain-toy
SKEL_SOFTSYNC_PREFILL=0 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
SKEL_SOFTSYNC_PREFILL=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

不要请用户跑 PKE Decrypt `-r npu`。干活 subagent **不自主推送**。
