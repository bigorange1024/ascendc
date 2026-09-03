# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：新 Cloud / 本地 Agent 的**唯一短真相**；本文件优先于长对话历史。  
> **入口**：[`AGENTS.md`](AGENTS.md) → **本文件** → Rule / Skill。  
> **最后刷新**：2026-09-03（Decrypt hang T0/T1 已沉；TASK-010 缺 slot0）

---

## ★ 给新 Agent 的 60 秒上手

1. 分支：**`cursor/kem-2launch-sticky-1534`**。  
2. **当前主控焦点 = PKE Decrypt 卡死**，不是再请用户上机 Encaps/Decrypt。  
3. 三张图：**Encrypt hang** / **Decrypt hang** / **Decaps K=131** — 禁止混图。  
4. 干活：主控设计；**同时仅一个 subagent**；SIM only。

### 待办快照

| 项 | 说明 |
|----|------|
| **P0** | **TASK-010**：`SKEL_OMIT_SLOT0`（AIV0 不写哨兵）预期 SIM 124；收 FB 刷新图谱 |
| **P0** | Decrypt hang 再下一刀：slot1 / 脏 GM（`DECRYPT_HANG_PLAN.md` T3/T4） |
| **P1** | Encaps：用户有空再 NPU Hostμ；**不要**用它顶替 Decrypt toy |
| **P1** | Decaps K=131：与 hang 正交 |

---

## ★ 当前真相

| 项 | 状态 |
|----|------|
| **Decrypt hang 图** | W1：`docs/rg-kem-decrypt-hang.yaml`；T0/T1 已沉 |
| **Decrypt hang 现场** | 用户：`prod input = dk_pke + c + lut_*` 之后卡死 → **PKE fused**，非 K=131 |
| Decrypt toy | **TASK-009 PASS**：合法 wall 3.934s；`OMIT_SET4=1` **124** |
| Encaps Hostμ | SIM 绿；`D-await-npu-host-mu`（**非本线阻塞**） |
| Decaps K | TASK-008 SIM 绿 max=0；另图 |
| graph_tests | TASK-001..009 闭环；**010 进行中** |

Decrypt fused 握手（须 toy 同构）：SoftSync slot0 → SET(4)/WAIT(8) → stub NTT 1/3 → slot1 → SET(4)/WAIT(8) → stub INTT 1/3（无 flag 2）；AIC 入口 Wait(4)。

已借入 Encaps：**缺 SET(4)⇒SIM 124**；**双 Cube 不是充分 hang 因**。

---

## ★ 下一刀（Cloud / subagent，不是用户 NPU）

```bash
# TASK-010（缺 slot0 Arrive）
cd ascendc-tests/fix-decrypt-skel-mix-chain-toy
SKEL_OMIT_SLOT0=0 SKEL_OMIT_SET4=0 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
KERNEL_COMPUTE_BUDGET_SEC=60 SKEL_OMIT_SLOT0=1 SKEL_OMIT_SET4=0 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

**不要**把下面当成 Decrypt hang 的下一刀：

```bash
# ❌ 未沉 toy 机制前不要请用户跑
cd examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-decrypt-k4
bash run.sh -r npu -v Ascend910B4
```

干活 subagent **不自主推送**。
