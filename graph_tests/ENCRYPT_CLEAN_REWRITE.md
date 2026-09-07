# 干净重写 Encrypt — 加法 PHASE（从推理图谱导出）

> 主控维护。落地：`ascendc-tests/fix-encrypt-clean-hostmu-2launch/`  
> 对照：`docs/rg-kem-encrypt-hang.yaml`  
> **方法（2026-09-04）**：**做加法**；每档 SIM 绿才进下一档；**减法改 stable 不作 hang 主路径**。  
> **不上机**，直到能回答「如何写才不卡」或写出整份 `NPU_SUITE.md`。

---

## 0. 加法纪律（强制）

| 规则 | 说明 |
|------|------|
| 一档一事 | 每 PHASE 只加**一类**能力（同步不变量 / 真计算块 / TRACE / I/O） |
| 绿门 | 本档 `SIM_DIRECT=1 bash run.sh -r sim` **PASS** 才开下一档；红则停在本档排 |
| 决策树 | 每档写明：**过 → 下一档**；**不过 → 回退/缩小本档**（禁止跳档碰运气） |
| 带着问题写 | 空 TRACE、Wait(4)↔SET(4)、禁 PrefixEmbed@设备、禁 SyncAll@Wait、固定 2-launch |
| 禁零散上机 | 加法未穷尽前不请用户 NPU |

---

## 1. 必须遵守的已证知识

| 节点 | 设计约束 |
|------|----------|
| `J-empty-trace-aic-wait4` | skipNtt：AIC `Wait(4)` 前 AIV **必须**可达双 AIV `SET(4)` |
| `F-omit-set4-hangs-sim` | 缺 SET(4) ⇒ 挂；**SET(4) 可达**为第一不变量 |
| `F-host-mu-ok-sim` / Hostμ | **μ 默认 Host**；设备 skipNtt **无** `PrefixEmbedMu` |
| `D-forbid-syncall-while-wait` | 禁止 AIC Wait 中 SyncAll |
| `D-softsync-follow-decrypt` | 若需 AIV 汇合，只跟 SoftSyncArrive 定式 |
| `D-reject-correctness-antipattern` | 禁滥 launch、碎写 GM 当正解 |
| `D-verify-sim-for-npu` | 迭代只认 SIM |

## 2. 已证伪（不要写回设计）

- 仅拆双 Cube / 仅加 launch 消粘  
- stub 下仅 1/3、仅合法 GATE、仅更大 Cube 为挂因充分条件  
- AIC Wait 时 SyncAll、自造 SoftSync 双向汇合  

## 3. 目标拓扑（干净 2-launch）

```
Host:
  Launch-1  prep+NTT（一轮 Cube；无设备 μ）
  HostFold: e2 += μ (mod q)     # 结构默认
  Launch-2  l18 skipNtt：at_jp → SET(4) → GATE → INTT → pack
Device L2:
  AIC: Wait(4) → Set(8) → Wait(1) MMAD Set(3) …
  AIV: （无 PrefixEmbed）at_jp → 双 AIV Set(4) → Wait(8) → …
```

## 4. PHASE 决策树（加法）

| PHASE | 加什么 | SIM 验收 | 过 → | 不过 → |
|-------|--------|----------|------|--------|
| **P0** ✅ | 2-launch + Hostμ + skipNtt 无 PrefixEmbed；stub at_jp；Wait(4)/SET(4) | magic `CLNENC01` / out[8] | **P1a** | 修骨架握手 |
| **P1a** ✅ | L2 **早 TRACE**（SET(4) 前/后可 Host 读） | magic `0x2A` + TRACE AIV 0–2 非空 | **P1b** | 缩 TRACE 写法 / 查 GM 可见性 |
| **P1b** | 加长 at_jp stub（仍非真内积；加压「SET 前工作量」） | SIM 绿；对照缺 SET(4)⇒124 | **P1c** | 缩 stub；确认未挡 SET(4) |
| **P1c** | 接入 **真 at_jp**（`library/shared`，禁 frozen） | SIM 绿 + 分阶段/golden | **P1d** | 回 P1b；假绿三问 |
| **P1d** | 真 INTT + pack | SIM 绿 + I/O | **P2** | 回 P1c |
| **P2** | prep+NTT 真实化；对齐 stable I/O | SIM 对拍 | 写 `NPU_SUITE` | 停在 P2 排 |

当前指针：**P1a ✅ → 下一刀 P1b**。

## 5. 与 stable 热修的关系

- stable `F203_HOST_FOLD_MU` = 热修旁路。  
- 本树把同一约束写成**默认结构**。  
- hang 主迭代在本树；stable 减法不是主路径。

## 6. 非目标

- 不解决 Decaps K=131。  
- 不引入 correctness 反模式。  
- 不在加法中途请零散上机。
