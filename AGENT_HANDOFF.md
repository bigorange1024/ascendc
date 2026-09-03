# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：新 Cloud / 本地 Agent 的**唯一短真相**；本文件优先于长对话历史。  
> **入口**：[`AGENTS.md`](AGENTS.md) → **本文件** → Rule / Skill。  
> **最后刷新**：2026-09-03（TASK-007/008 闭环；**等用户 NPU**：Encaps Hostμ + Decaps A/B/C）

---

## ★ 给新 Agent 的 60 秒上手

1. 分支：**`cursor/kem-2launch-sticky-1534`**（未授权勿推送）。  
2. Encaps：**Host 折 μ** SIM 绿；clean P0 探针 SIM 绿；**粘性等 NPU**。  
3. Decaps：**Cloud 默认 SIM 仍绿**（TASK-008）；K=131 **专攻 NPU**（H4）。  
4. 图谱：encrypt / decrypt 两份 yaml；章程 `graph_tests/CHARTER.md`。

### 待办快照

| 项 | 说明 |
|----|------|
| **P0** | 用户 NPU：Encaps 默认 Hostμ 加压 + `F203_HOST_FOLD_MU=0` 对照 |
| **P0** | 用户 NPU：Decaps 默认 + A/B/C；红样本 K vs J(z‖c) |
| **P1** | clean Encrypt P1（真 at_jp/INTT）— 可排期，勿与 NPU 反馈抢 |

---

## ★ 当前真相

| 项 | 状态 |
|----|------|
| Encaps Hostμ | SIM 绿；`D-await-npu-host-mu` |
| clean P0 | `fix-encrypt-clean-hostmu-2launch` SIM 绿（~3.6s）；`F-clean-p0-sim-pass` |
| Decaps | SIM 绿 max=0；K=accept≠J；`D-user-npu-abc` |
| graph_tests | TASK-001..008 闭环（007/008 已 FB） |

---

## ★ 下一刀（用户 NPU）

```bash
# Encaps Host μ
cd examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-encaps-k4
KEM_ENCAPS_FORCE_REBUILD=1 bash run.sh -r npu -v Ascend910B4
# 连跑勿每轮 FORCE；对照 F203_HOST_FOLD_MU=0

# Decaps（FORCE 一次后）
cd ../stable-fips203-mlkem-kem-decaps-k4
KEM_DECAPS_FORCE_REBUILD=1 bash run.sh -r npu -v Ascend910B4
# A/B/C 见 DECRYPT_K131_PLAN / qa§6；红则比 K vs J(z||c)
```

可选一并上机：`ascendc-tests/fix-encrypt-clean-hostmu-2launch`（P0 magic）。

**不自主推送**。
