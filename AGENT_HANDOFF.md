# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：新 Cloud / 本地 Agent 的**唯一短真相**；本文件优先于长对话历史。  
> **入口**：[`AGENTS.md`](AGENTS.md) → **本文件** → Rule / Skill。  
> **最后刷新**：2026-09-03（**Decrypt hang 单独开图 + TASK-009**；Encaps 等 NPU 不挡本线）

---

## ★ 给新 Agent 的 60 秒上手

1. 分支：**`cursor/kem-2launch-sticky-1534`**。  
2. **当前主控焦点 = PKE Decrypt 卡死**，不是再请用户上机 Encaps/Decrypt。  
3. 三张图：**Encrypt hang** / **Decrypt hang** / **Decaps K=131** — 禁止混图。  
4. 干活：主控设计；**同时仅一个 subagent**；SIM only。

### 待办快照

| 项 | 说明 |
|----|------|
| **P0** | **TASK-009**：`fix-decrypt-skel-mix-chain-toy` 合法握手 SIM 绿 + `OMIT_SET4` 预期 124；收 FB 刷新 `rg-kem-decrypt-hang.yaml` |
| **P0** | Decrypt hang 下一刀：OMIT_SLOT0 / slot1（见 `DECRYPT_HANG_PLAN.md` T2/T3） |
| **P1** | Encaps：用户有空再 NPU Hostμ；**不要**用它顶替 Decrypt toy |
| **P1** | Decaps K=131：与 hang 正交；勿在 hang TASK 里改 |

---

## ★ 当前真相

| 项 | 状态 |
|----|------|
| **Decrypt hang 图** | W0：`docs/rg-kem-decrypt-hang.yaml`；计划 `graph_tests/DECRYPT_HANG_PLAN.md` |
| **Decrypt hang 现场** | 用户：`prod input = dk_pke + c + lut_*` 之后卡死 → **PKE fused**，非 K=131 |
| Decrypt toy | **尚未建**（TASK-009） |
| Encaps Hostμ | SIM 绿；`D-await-npu-host-mu`（**非本线阻塞**） |
| Decaps K | TASK-008 SIM 绿 max=0；另图 |
| graph_tests | TASK-001..008 闭环；**009 进行中** |

Decrypt fused 握手（须 toy 同构）：SoftSync slot0 → SET(4)/WAIT(8) → stub NTT 1/3 → slot1 → SET(4)/WAIT(8) → stub INTT 1/3（无 flag 2）；AIC 入口 Wait(4)。

已借入 Encaps：**缺 SET(4)⇒SIM 124**；**双 Cube 不是充分 hang 因**。

---

## ★ 下一刀（Cloud / subagent，不是用户 NPU）

```bash
# 主控已下发；subagent 在用例目录串行 SIM（禁并行）
# cd ascendc-tests/fix-decrypt-skel-mix-chain-toy
# SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
# KERNEL_COMPUTE_BUDGET_SEC=60 SKEL_OMIT_SET4=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

**不要**把下面当成 Decrypt hang 的下一刀：

```bash
# ❌ 未沉 toy 机制前不要请用户跑
cd examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-decrypt-k4
bash run.sh -r npu -v Ascend910B4
```

干活 subagent **不自主推送**。
