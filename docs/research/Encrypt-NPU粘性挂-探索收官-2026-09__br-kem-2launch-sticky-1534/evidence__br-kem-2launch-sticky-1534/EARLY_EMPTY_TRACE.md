# EARLY：空 TRACE（0/16）收窄分析（2026-09-07）

**触发**：NPU-3/N14 — Encaps `F203_L18_TRACE=1` 挂时 `[l18-trace] stages set=0/16 :`（D2H 成功、槽全 0）。  
**前提**：Host 已 print `launch 2 f203_encrypt_l18_l19`；prep launch 已成功回。

---

## 1. TRACE 管道是否可信？

| 检查 | 结论 |
|------|------|
| `F203_L18_TRACE=1` → `traceDev` malloc + 传入 launch | ✅ Host 日志有「将轮询 fused-trace」 |
| 挂时出现 `[l18-trace] stages set=0/16` | ✅ **D2H 成功**（失败会静默 `continue`，不打印） |
| 成功轮次（~270µs）无 `[l18-trace]` 行 | ✅ **正常**：poll 默认 500ms，核已结束；**不能**用「成功轮无 TRACE」反推传参坏 |

⇒ **空槽是真实设备内容（全 0），不是 Host 假阴性管道。**

---

## 2. 核内首标时间线（只读）

`f203_encrypt_l18_l19`（`FusedTraceMark`：仅 **AIC** 或 **AIV0** 写）：

```
AIV0:  [PrefixEmbedMu] → Mark(15 MU_E2) → Barrier → AivK8Split → Set(1) → Mark(0 NTT_SPLIT) → …
AIC:   Wait(1) → MMAD → Mark(1 NTT_MMAD) → Set(3) → …
AIV1:  （不写 TRACE）Split → Set(1) → …
```

**0/16 且无下标 15** ⇒ AIV0 **从未完成** `Mark(15)`。  
同时 AIC 也未到 `Mark(1)`（合理：卡在 `Wait(1)` 或未调度）。

### 已排除（相对「挂在 GATE/INTT」）

| 已排除 | 理由 |
|--------|------|
| GATE 4/8 中段 | 需先过 Mark 0/1/3/4/5… |
| INTT 复用 1/3 | 更后；且 E17/E18 实机×11 绿（N12） |
| 「卡在某业务 TRACE 号之间」 | 没有任何号 |

### 仍可能的挂窗（EARLY）

| ID | 假说 | 含义 |
|----|------|------|
| **H-E1** | AIV0 死在 **PrefixEmbedMu**（含其 TPipe/GM）内 | 有 m/e2 时 Mark15 在前缀**之后**；未标 ⇒ 前缀未完成 |
| **H-E2** | 若某路径跳过前缀：死在 **AivK8Split** 完成前 | 当前 Encaps Host **总是**传非空 m/e2 → 本路径更贴 H-E1；仍保留 |
| **H-E3** | **核未真正在核上推进**（stream/设备态/调度粘） | launch 文案已打，但 AIV0/AIC 无有效进度 |
| **H-E4** | 多轮后 **UB/TPipe/工作区** 耗尽或损坏，首段 GM/TPipe 即挂 | 对齐「第 N 轮才挂」；E17 stub 无此负载故不挂 |
| **H-E5** | prep→l18 **数据面**（y/e/ws）在第 N 轮异常，使 Split/前缀挂 | 需对照 prep 输出；禁当正确性刀 |

**相对排序（当前）**：H-E1 / H-E4 ≻ H-E3 ≻ H-E5 ≻ H-E2；**H-reuse 已降权**。

---

## 3. 与已绿对照的差（为何 E17 不挂）

| | E17 stub | Encaps `l18_l19` |
|--|----------|------------------|
| 负载 | 空握手 | PrefixEmbedMu + 真 Split/MMAD/at_jp |
| 首指令后 | 很快 Set/Wait | 先做 μ 嵌入与 NTT Split |
| 多 launch | 单核反复 | **prep + l18** 两段 |
| 实机×11 | 绿 | r11 挂且 0/16 |

⇒ 粘性充分条件更可能在 **「重 AIV 前缀/Split + 多轮/污染」**，不在「flag 布局 stub」。

---

## 4. 离线实验矩阵（EARLY）

| ID | 做什么 | 目的 | 上机？ |
|----|--------|------|--------|
| **E19** | stub：AIC/AIV0 **入口第一句** Mark 固定槽（如 15 与 0 之前另开「entry」语义槽或复用打印协议）；再跑现有 1/3 序 | SIM 证「极早标」可被 Host 轮询看到；供日后 NPU 区分 H-E3 vs H-E1 | 日后授权 |
| **A-host** | 静态核对（本文件 §1）：传参/poller/成功轮无行 | ✅ 本波完成 | 否 |
| **A-prefix** | 只读 `PrefixEmbedMuIntoE2Gm` / Split 入口：有无显式死等、异常大循环 | 降 H-E1 代码面风险 | 否 |
| **E20**（可选） | 单进程多轮：**空 stub ×N 后** 再跑「仅 PrefixEmbedMu 量级」toy（仍禁抄全 Encrypt） | 逼近 H-E4 | SIM 先 |

**禁止**：未 EARLY 钉窗就改 CrossCore FSM；复测仅 GATE/仅 reuse1/3。

### 日后上机（须授权）· 反馈分支

| 反馈 | 计划 |
|------|------|
| E19 入口标有、业务标仍 0 | 死在入口标之后、Mark15 前 → **H-E1** 聚焦 Prefix |
| E19 入口标也没有 | **H-E3** 调度/stream；查多轮 ACL 态 |
| Encaps 改「Mark 挪到 Prefix **之前**」后挂时见新标、不见 15 | 坐实 Prefix 内挂 |
| 见 15 不见 0 | Split 窗（H-E2） |

---

## 6. 上机修订（2026-09-08 · N15–N17）

| 事实 | 含义 |
|------|------|
| E19 标量第 4 轮丢槽 15 | AIV **标量** fused-trace 对 Host D2H **多轮不稳** |
| E19b DataCopy×12 绿 | 整表 RMW 可修观测 |
| Encaps DataCopy 后挂时 **16/16** | 原 **0/16 空槽是观测假象**；设备打点已齐 |
| 同波 0 PASS | 真 Sync 挂仍在；且 TRACE-DC 实现可能加重不稳 → 须干净卡对照 |

**挂窗改标**：优先查 **最后一次 FusedTraceMark 之后**（`tail_pack_shard_gm`、stream Sync、污染），**不再**默认「Prefix 前 EARLY 死」。

---

## 5. 一句话（修订）

**空 TRACE 曾把分析带偏到「入口前」；E19b/Encaps-DC 证明标量 Mark 不可靠。真粘性挂在 TRACE 已满时仍可 Sync 未回 → 查末段 tail/Sync，并隔离 TRACE 探针对正确性/挂率的干扰。**
