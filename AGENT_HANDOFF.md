# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：新 Cloud / 本地 Agent 的**唯一短真相**；本文件优先于长对话历史。  
> **入口**：[`AGENTS.md`](AGENTS.md) → **本文件** → Rule / Skill。  
> **最后刷新**：2026-09-03（文档：实机 TRACE 空槽 + SPLIT 亦挂；**代码未改推**；失败 SyncAll/SoftSync 已回退）

---

## ★ 给新 Agent 的 60 秒上手

1. 分支：**`cursor/kem-2launch-sticky-1534`**（勿只拉 main；未授权勿合 main）。  
2. Cloud 首次：`bash scripts/clone-thirdparty.sh`（含 liboqs）。  
3. **代码真相**：Encaps/Decaps 默认 2-launch 在分支上；Cloud **CPU+SIM 绿**（回退失败同步实验后复验 PASS）。  
4. **实机 Encaps（未闭环）**：  
   - 默认与 **`SPLIT_PREP`** 都粘；挂点含 **`prep_ntt` / `ntt_y` / `l18`** → **非 launch 次数**。  
   - `F203_L18_TRACE=1` 挂 l18 时 **`stages set=0/16` 空** → AIV 未写 μ/at_jp；AIC 疑死等 **`WAIT(4)`**。删 `out*` 后亦可复现。  
   - 详 [`qa/2026-09/2026-09-03-…`](qa/2026-09/2026-09-03-Encaps-Decaps真2launch与GATE.md)。  
5. **Decaps** `K max=131` 另线。写 AscendC 前：Rule + engineering-notes + API 查阅索引。

### 刚关闭 / 已证伪（勿再试）

| 项 | 说明 |
|----|------|
| 「只拆双 Cube 即消粘」 | FORCE 后仍 3–5 轮挂；SPLIT 亦挂 |
| 「Encrypt prep 不能进 MIX」 | `PREP_MIX_ONLY` PASS；缺 GATE 才红 |
| **AIC 仍 Wait 时 `SyncAll`** | 违查阅索引「AIC 已返回」；SIM 互锁 |
| **自造 SoftSync 双向汇合 / 默认 recreate stream** | SIM l18 挂；已回退源码到分支干净 2-launch |

### 待办快照

| 项 | 说明 |
|----|------|
| **实机 Encaps 粘性** | 空 TRACE → 极早段；对照 KeyGen；入口 TRACE **未合入代码** |
| **实机 Decaps K≠0** | `F203_DECRYPT_FUSED` / `SPLIT_PREP` / 双 FUSED |
| **stable-512/768** | 须用户 `#交付#` |

---

## ★ 接手清单（2026-09-03）

| 优先 | 项 | 做什么 | 注意 |
|------|----|--------|------|
| **P0** | **Encaps 实机定位** | 空 TRACE + KeyGen 对照；改码前对 SyncAll 索引 / Decrypt SoftSyncArrive | **禁止** AIC Wait 中 SyncAll；勿再盲试汇合 |
| **P0** | **Decaps K=131** | 正交 FUSED / SPLIT | 与粘性正交 |
| **P1** | 可选下一刀（须确认） | Host 折 μ + 入口 TRACE 仅一刀，先 Cloud SIM | 未授权不 push 代码 |

**别做**：未授权 commit/push 代码；从 frozen 抄码；`#交付#` stable-512/768。

---

## ★ 当前真相

| 项 | 状态 |
|----|------|
| **1024 Encaps/Decaps 代码** | 默认真 **2-launch**；Cloud SIM 绿；**粘性未消** |
| **本轮文档推送** | qa + HANDOFF 诊断；**无代码 diff** |

---

## ★ 下一刀

**P0**：实机粘性根因仍开；资料约束：SyncAll 仅 AIC 已返回；SoftSync 跟 Decrypt SoftSyncArrive，勿自造。  
可选（须用户确认再写码）：Host 折 μ + 入口 TRACE → SIM → 再谈 push 代码。
