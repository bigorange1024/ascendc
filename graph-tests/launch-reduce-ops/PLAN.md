# Launch 压缩战役 · 全套 NPU 实验计划

> **状态**：计划已锁定；**待用户开机 NPU** 后立即执行。  
> **分支工作区**：当前检出可改；**禁止擅自 commit/push**（仅用户明确授权后）。  
> **验收真源**：仅 **NPU**（`-r npu`）。**禁止**为本战役新写/依赖 CPU、SIM 作结案依据。  
> **工具**：强制 `thirdparty/cannbot-skills`（至少 `ascendc-sync-audit`；经验总结走 plugin 口径落盘本目录）。  
> **空闲**：NPU 会话 **空转 ≤3 分钟**——刀间立刻下一刀或停 keepalive 放机。

---

## 0. 目标（已锁）

| 算子 | 现状（重建） | stable 参考 | **本战役目标** |
|------|--------------|-------------|----------------|
| PKE KeyGen | **3**（RB-K04） | 2（prep+mmad） | **→ 2** |
| KEM KeyGen | **4**（RB-K06） | 2 | **→ 2**（允许经 3 过渡） |
| KEM Decaps | **5**（RB-T26） | 3（fused Decrypt + Encaps 2） | **→ 3** |
| （中间）PKE Decrypt | 3（RB-D04） | 1（fused） | **→ ≤2**，优先试 **1** 以支撑 Decaps=3 |

口诀对照：Encaps 已能 **2**；KeyGen/Decaps 无理由长期停在 4/5。  
**≠** 抄 stable 算子树；只对齐 **launch 数与「短握手可融合」假说**，实现仍从重建树改。

**双门禁不变**：同 ISA **liboqs** I/O ∧ 干净卡 **NPU×30** 不挂（中间档）；超时预算 **180s/次**。

---

## 1. 为何现在可以压 launch（推理，对齐工程 KB）

1. Encaps 重建 = **AIV prep + 一次 MIX** → 证明「无 CC 前缀与 MIX 切开」后，**MIX 内部可承载多段 Cube**，不必再为每段 Cube 加 Host launch。  
2. KeyGen 多出来的 launch 主要是 **L2a NTT 与 L2b dot+encode 分核**、KEM 再加 **L3 AIV tail**——属于**保守拆分**，不是 Encaps 已证伪的「不可融合」。  
3. Decaps=5 = Decrypt3 + Encaps2；stable=3 ⇒ 关键杠杆在 **Decrypt 侧融合**（NTT∥INTT 分核是默认，但是**可破例**：须新决策 + NPU 证据）。  
4. 证伪约束仍有效：融合有效 = 消断裂/争用/半写；**不是**「加次数」的逆命题乱融。

---

## 2. 融合假说矩阵（按风险从低到高）

### 2.1 KeyGen

| ID | 变更 | launch | 风险面 | 先决 |
|----|------|--------|--------|------|
| **KG-F1** | 融 L2a+L2b → 单 MIX `kg_ntt_dot_encode`；保留 L1 prep | PKE **3→2** | 同核两轮 Cube；flag 复用须分段复位；禁 Wait 内 SyncAll | sync_audit clean；参照 Encaps compute 内多段模式 |
| **KG-F2** | 在 F1 的 MIX **AIV 收尾**嵌入 H(ek)+z+拼 dk_kem（原 L3） | KEM **4→2** | AIV epilogue 与 Cube GATE 次序；字符串/`uint8` 表；无新 CrossCore | F1 已 NPU×30 绿 |
| **KG-F2b**（退路） | F1 + 保留独立 L3 | KEM **4→3** | 低 | F1 绿但 epilogue 挂/错时 |

**禁止优先尝试**：prep∥NTT 同 launch（半写 Â 面，KB 高风险）。

### 2.2 Decrypt → Decaps

| ID | 变更 | launch | 风险面 | 先决 |
|----|------|--------|--------|------|
| **DC-F1** | 融 L2a+L2b（NTT+INTT）→ 单 MIX；保留 L1 prep | Decrypt **3→2** | **flag 争用**（历史主因）；须同核内两段握手**串行且 flag 生命周期隔离**（或第二段换号∈合法表） | sync_audit + 可达性分析写进 TASK |
| **DC-F2** | 再融 L1 prep 进该 MIX 前缀（AIV→再 MIX） | Decrypt **2→1** | prep 与 MIX 同核：半成品 û；须核内全局可见屏障（非 SoftSync） | DC-F1 ×30 绿 |
| **DP-F1** | Decaps = DC-F* Decrypt + 既有 Encaps **2** | **5→4 或 3** | Host 编排拼装；basename 不撞名 | Decrypt 目标档绿 |
| **DP-F0**（对照） | 仅测「Decrypt 仍 3 + Encaps 2」基线 | 5 | 无 | Wave0 |

**Encaps 2 不改**（已合理）；Decaps 压数不靠再拆 Encaps。

---

## 3. 波次日程（NPU 上板顺序）

### Wave 0 — 基线钉死（≤1 轮×各 1，再可选×5 冒烟）

| 刀 | 目录 | 期望 |
|----|------|------|
| W0-KG | `kg_related/RB-K04` / `RB-K06` | 现 3/4 launch 仍绿（确认远端树/liboqs） |
| W0-DC | `dec_related/RB-D04-decrypt-full` | 现 3 launch 仍绿 |
| W0-DP | `enc_related/RB-T26-decaps-device` | 现 5 launch 仍绿 |

失败 → **停压 launch**，先修环境/污染；不进入 F*。

### Wave 1 — KeyGen → 2（主攻）

1. 新目录 `kg_related/RB-K07-pke-2launch`（自 RB-K04 **编排+核合并**，禁抄 stable KeyGen）。  
2. **强制** cannbot：
   ```bash
   python3 thirdparty/cannbot-skills/ops/ascendc-sync-audit/scripts/sync_audit.py \
     --check all --format json <RB-K07目录> \
     > graph-tests/launch-reduce-ops/tasks/LR-KG-F1/logs/sync_audit.json
   python3 thirdparty/cannbot-skills/ops/ascendc-sync-audit/scripts/ascendc_flow_analyzer.py \
     <相关.cpp/.hpp> --frontend auto --format json \
     > graph-tests/launch-reduce-ops/tasks/LR-KG-F1/logs/flow.json || true
   ```
3. 远端：**只** `bash run.sh -r npu -v Ascend910B3`（或机型实测）；`KERNEL_COMPUTE_BUDGET_SEC=180`。  
4. 单轮 liboqs 绿 → **×30**；过则 KG-F1 **关**。  
5. 立刻 KG-F2（`RB-K08-kem-2launch`）或退路 F2b（`RB-K08-kem-3launch`）。

### Wave 2 — Decrypt 2 → Decaps 4

1. `dec_related/RB-D08-decrypt-2launch`（DC-F1）。  
2. 同上 sync_audit / flow；NPU 单轮→×30。  
3. `enc_related/RB-T28-decaps-4launch` = D08 + Encaps2。

### Wave 3 — Decrypt 1 → Decaps 3（对齐 stable 数）

1. `RB-D09-decrypt-1launch`（DC-F2）——**最高风险**；挂则停并写证伪/条件，不硬融。  
2. `RB-T29-decaps-3launch`。  
3. 算子级加压：目标档 **×30 必过**；有余力再 ×100。

### Wave 4 — 经验入库（不推送除非授权）

- 更新 `docs/notes/ascendc-engineering-kb.md`：§2 破例条件是否满足、新决策草案。  
- 图谱：若确认「同核串行两段 MIX + flag 隔离 ⇒ 可 2/1 launch」→ 新 `D-*` / `J-*`（须用户后批图改也可先写草案）。  
- `.cannbot/launch-reduce/插件经验总结.md`（踩坑 / 耗时 / sync_audit 红线）。  
- 当日 `qa/` 追加。

---

## 4. 硬门禁（每刀）

| # | 门禁 |
|---|------|
| 1 | **runner = NPU only**；不把 CPU/SIM 绿当过关 |
| 2 | sync_audit：**无红线**才上板；有红线须修或书面降级理由 |
| 3 | flag∉{5,7}；禁 SoftSync；AIC Wait 环禁 SyncAll |
| 4 | 业务写出 UB+DataCopy；设备字符串 `uint8` 表 |
| 5 | 权威 liboqs 同 ISA；禁 python 冒充 |
| 6 | 禁抄 `examples/**/*keygen*`、`*decaps*` stable 核、frozen |
| 7 | basename 全局唯一（防 507000） |
| 8 | 干净卡；杀挂后不解释首次挂；刀间空闲 **&lt;3 min** |
| 9 | **无用户指示不 commit/push** |

---

## 5. NPU / 心跳协议

```text
开机信号 → which_npu 成功 → 启动 keepalive（≤40s）
         → 立刻 Wave0（连续跑，刀间隔目标 &lt;60s）
         → Wave1… 编码尽量在上刀 NPU 跑着时本地改下一刀
         → 任一长空档将至 3min：要么开下一命令，要么停 keepalive 放机
睡觉无人值守：NPU 不可达时 **不** 启 keepalive；仅轮询连通性
```

远端工作树：优先复用既有 `/mnt/workspace/ascendc-keygen`（或 encrypt 树）+ bundle/rsync；**先确认 liboqs 链与 `ASCEND_DEVICE_ID=0`**。

---

## 6. cannbot 使用清单（强制）

| 时机 | Skill / 脚本 |
|------|----------------|
| 每刀改同步相关码后 | `ops/ascendc-sync-audit` full-audit（`sync_audit.py` + `ascendc_flow_analyzer.py`） |
| 挂死 | `workflows/deadlock-triage.md` |
| 波次结束 | `plugin-experience-summary` 口径 → 本目录 `.cannbot/` |
| 编码实现 | 主控可派 subagent **只写码**；**禁止** subagent SSH/NPU；主控独占上板 |

不走完整 ops-direct-invoke 问卷流（用户已睡 + 已锁目标）；等价 **silent**：以本 PLAN 为 CP1/CP2 已确认件。

---

## 7. 成功 / 失败判定

| 结果 | 定义 |
|------|------|
| **成功** | PKE-KG=2 ∧ KEM-KG=2 ∧ Decaps=3，均 liboqs + NPU×30 |
| **有条件成功** | KG=2 且 Decaps=4（Decrypt=2）且 Decaps=3 因 DC-F2 证伪停住——须写入 KB |
| **失败** | Wave0 基线挂；或 F1 级融合系统性不可达 Set 且无隔离方案 |

---

## 8. 文件地图

| 路径 | 用途 |
|------|------|
| 本文件 `PLAN.md` | 总计划（本篇） |
| `QUEUE.md` | 刀状态 |
| `COMMON.md` | 本战役约束（覆盖旧「先 CPU」） |
| `tasks/LR-*/` | 单刀 TASK/FEEDBACK/logs |
| `kg_related/RB-K07*` / `RB-K08*` | KeyGen 降 launch 实现 |
| `dec_related/RB-D08*` / `RB-D09*` | Decrypt 降 launch |
| `enc_related/RB-T28*` / `RB-T29*` | Decaps 拼装 |

---

## 9. 当前阻塞

- **2026-09-09 19:01 探活**：`which_npu` / SSH `cannlab-npu` **失败**（hostname 不可解析；tailscale 协调服异常）。  
- **处置**：写完计划与队列后进入短周期探活；**NPU 未上线前不启 keepalive、不上板**。
