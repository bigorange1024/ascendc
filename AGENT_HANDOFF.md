# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：新 Cloud / 本地 Agent 的**唯一短真相**；本文件优先于长对话历史。  
> **入口**：[`AGENTS.md`](AGENTS.md) → **本文件** → Rule / Skill。  
> **最后刷新**：2026-09-03（Decrypt hang T0/T1 沉；T2 空 while SIM 不挂；TASK-011 T3）

---

## ★ 给新 Agent 的 60 秒上手

1. 分支：**`cursor/kem-2launch-sticky-1534`**。  
2. **当前主控焦点 = PKE Decrypt 卡死 toy/SIM**，不是请用户上机。  
3. 三张图：Encrypt hang / **Decrypt hang** / Decaps K=131 — 禁止混图。  
4. 干活：主控设计；**同时仅一个 subagent**；SIM only。

### 待办快照

| 项 | 说明 |
|----|------|
| **P0** | **TASK-011**：第一轮 GATE 合法、第二轮省略 SET(4) 预期 SIM 124 |
| **P1** | 勿再用空 `while(s==0)` 在 SIM 上硬凑 SoftSync hang（已 retracted） |
| **P1** | Encaps Hostμ NPU / Decaps K=131：正交，不挡本线 |

---

## ★ 当前真相

| 项 | 状态 |
|----|------|
| Decrypt hang 图 | `docs/rg-kem-decrypt-hang.yaml` |
| 现场 | `prod input = dk_pke + c + lut_*` 之后卡死 → **PKE fused** |
| T0 | 合法握手 SIM 绿 wall≈3.2–3.9s magic `SKELDEC1` |
| T1 | `OMIT_SET4=1` → **124**（CrossCore Wait(4)）**verified** |
| T2 | `OMIT_SLOT0=1` 宏已生效，SIM **仍绿** → 空 while **不是** SIM hang 代理 |
| Encaps / K=131 | 另图；非本线阻塞 |

---

## ★ 下一刀（Cloud / subagent，不是用户 NPU）

```bash
cd ascendc-tests/fix-decrypt-skel-mix-chain-toy
SKEL_OMIT_SET4_R2=0 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
KERNEL_COMPUTE_BUDGET_SEC=60 SKEL_OMIT_SET4_R2=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

不要请用户跑 `stable-fips203-mlkem-pke-decrypt-k4 -r npu`。干活 subagent **不自主推送**。
