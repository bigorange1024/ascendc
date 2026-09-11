# AscendC 工程知识库（写码用）

> **读者**：要在本仓写/改 ML-KEM AscendC（KeyGen / Encrypt / Decrypt / Encaps / Decaps）的人与 Agent。  
> **目标**：只靠本文件 + 机读图，能推出「怎么写才对、怎么验才算过、哪里绝对不能走」。  
> **图谱**：[`docs/rg-ascendc-engineering.yaml`](../rg-ascendc-engineering.yaml) · 重渲 `python3 scripts/rg_viz.py`  
> **长文（数学/平台深挖）**：见文末「延伸阅读」；**战役流水**不在本文 → `graph-tests/*-rebuild-ops/`、`AGENT_HANDOFF.md`、`qa/`。

---

## 0. 怎么用本 KB

| 你要做的事 | 先读 |
|------------|------|
| 开新 MIX / 多 launch 路径 | §1 机制 → §2 默认决策 → §6 开路径检查单 |
| 对拍绿但心里没底 | §4 假绿阶梯；勿跳级结案 |
| NPU 超时 / SynchronizeStream 不回 | §1.1 + §1.2；修可达性，勿先加 launch 次数 |
| 结果字节错 / CPU 绿 NPU 红 | §3 正确性钉子（写出 / ISA / liboqs） |
| 想「融回去省 sync」 | §2 `D-SHORT-CROSSCORE` 破例条件；须**新决策**，禁默默融 |
| 查刀号、×30、哪天绿了 | **别查本文** → QUEUE / MATRIX / HANDOFF |

**双门禁**（`D-DUAL-GATE`）：权威 I/O 正确 **∧** NPU 加压不挂。证其一不蕴含另一。

**入库标准**（`D-ADMIT-RETESTABLE`）：能指导下次写码的机制 / 证伪 / 带代价权衡。现象台账、目录索引、launch 拼法日记 → 不进 KB 主文。

---

## 1. 机制（推理主链）

图谱主干：`J-HANG-UNREACHABLE-SET` → `J-HALF-TABLE-BLOCKS-SET` / `J-FLAG-CONTENTION` → `J-FUSION-EXPANDS-SURFACE` → `J-SHORT-CROSSCORE-HELPS-BOTH`。

### 1.1 跨核挂 ≈ Wait 而对端不可达 Set

| 前提 | 结论 | 写码含义 |
|------|------|----------|
| CrossCore `Wait(f)` 要求对端**本次**路径可达 `Set(f)` | Host 长期不回的主因是可达性断裂（含 Wait 环内再阻塞） | 排挂：画「谁 Wait、谁本应 Set、那条路径是否必达」；**不是**先加 Host launch 次数 |

可复验：人为省略配对 SET → SIM/结构路径可 timeout。

### 1.2 半写共享表 → 永不 Set **或** 半成品错数

多写者共享 Â / CBD 等全局表，下游却假定「已满」且无 Host/全局屏障 → 消费者永远到不了 Set，**或**带着半边数据继续算。

写码含义：prep∥NTT 同 launch 无屏障 = 同时放大挂面与错数面（Decrypt 史）。

### 1.3 同组 flag 被两段 MIX 争用

NTT 与 INTT（或两段深 GATE）共用 flag 1–3 且同 launch/无隔离 → 错数或互锁史。

写码含义：分 kernel + Host mid-sync，各自短握手面。

### 1.4 深融合扩大握手面与半成品面

把无 CrossCore 的 prep 与有 CrossCore 的 MIX、或 NTT 与 INTT，融进单长核 → 同时放大「到不了 Set」与半成品错。

拆 launch + mid-sync：**用屏障换更短握手**（代价：更多 Host sync / 可能更高墙钟）。  
收窄握手是**正确∧不卡的共同地基**，不是「只为不挂的额外手续」（`J-SHORT-CROSSCORE-HELPS-BOTH`）。

### 1.5 卡死 vs 算错（勿混排错）

| 类型 | 现象 | 优先查 |
|------|------|--------|
| 卡死 | `aclrtSynchronizeStream` 不回 / timeout | §1.1–1.4、硬禁 sync |
| 算错 | Stream 回但对拍失败 | §3 写出 / 布局 / 权威 ISA；半写表也产错数 |

---

## 2. 默认怎么选（设计权衡）

每条：**为何 → 代价 → 何时可破**。破例须在图上立**新决策**，禁止默默融回旧面。

| 决策 | 为何 | 代价 | 何时可破 |
|------|------|------|----------|
| **硬禁** flag 5/7；AIC 在 CrossCore Wait 中禁 SyncAll/IBWait；禁 SoftSync | 直接制造不可达 Set / 互锁 | 无 | **无** |
| **默认短 CrossCore**：无 CC 的 prep 与 MIX 分 launch；NTT 与 INTT 分核；每次 launch 后 mid-sync | 融合扩大握手与半成品面 | 更多 Host sync | 握手已极短、无半写、且有 NPU×N + 性能数据 → 新决策 |
| flag 默认 ∈ {1,3,4}（1/3 Cube + 4 GATE） | 短表够用；避开 5/7 | 表达力受限 | 单独论证 GATE 与 Wait 可达性 |
| 共享全局表默认 **单写者**（`BLOCK_DIM=1` / AHAT=1） | 半写→永不 Set 或错数 | 放弃 prep 双 AIV tick | 分片无交叠 + Inline SHAKE 语义已证且有收益 → 新决策 |
| Decrypt：NTT∥INTT **分核** | flag 争用史 | 相对 fused +launch | 同短 CrossCore 破例 |
| Decaps / 多段往返：一段一边界 + mid-sync | 同 session 无屏障会污染后续段 | 更多 launch | 禁止用「两段 aclFinalize」当生产基线 |
| 结构假说先 **SIM**；粘性/污染挂只认**干净卡 NPU** | SIM≠粘性 | 排程更重 | — |
| 不挂门禁 **NPU×30**（中间）/ **×100**（算子级）；预算约 180s/次；超时=缺陷 | 单轮绿不够；须排除杀挂污染 | 占卡 | — |
| 业务 GM：**UB + DataCopy**；权威：**同 ISA liboqs** | SetValue / 异架构假绿 | — | — |
| NTT：**poly-batch**；limb 用算术低位；S1–S3 **禁 Gather** | 数学契约 | — | 换参数组须显式勾差清单 |
| 禁整文件 fork 现 Encrypt `l18` / 旧 alg15·21 深 GATE 当模板 | 与粘性挂史高度相关（`Q-ENCRYPT-STICKY` 仍开） | 须自拼短握手 | 可参考契约与反卡死原则，禁抄同步 FSM |

图谱节点：`D-HARD-SYNC-BANS` … `D-KEM-MATH-INVARIANTS`、`D-NO-FORK-L18`。

---

## 3. 正确性钉子（与「不卡」并列）

### 3.1 权威与假绿

- 交付正确性 = 与**同 ISA**权威（liboqs）I/O 等价；golden 是 oracle，**不要求**与参考源码同构。  
- 缺 liboqs → **BLOCKED**，禁止 python 冒充权威结案（1024 无 thirdparty 的跑通例外见 AGENTS，权威交叉仍要装库）。  
- **禁止**把开发机 x86 的 `liboqs_*_ref` 覆盖到 aarch64 NPU 机。

### 3.2 写出路径

- 业务结果写 GM：经 UB + `DataCopy`（或已证明等价路径）。  
- 依赖 `GlobalTensor::SetValue` → 常见 **CPU 对拍绿、NPU 错字节**。

### 3.3 设备常量字符串

- `__aicore__` 内 C 字符串字面量是 `const __gm__ char[]`，不可赋给 `const char*`（CPU 可过、NPU 编不过）。  
- 用 `constexpr uint8_t[]` 再入 UB。

### 3.4 NTT / poly 契约（摘）

| 不变量 | 要点 |
|--------|------|
| poly-batch | Stage2 后每个 AIV 握有完整 poly 的 hi+lo；**禁** limbsplit |
| limb 低位 | `lo = v - (v>>6)*64`；**禁** `And(v,63)` |
| Gather | **仅** Tag5T 三段式 NTT S1–S3 禁；其后 basemul/ByteEncode 另议 |

深文：`MLKEM-NTT-实现总结.md`、`MLKEM-NTT-向量与标量实现指南.md`。

---

## 4. 假绿阶梯（结案用）

| 绿了什么 | 能证明 | 不能证明 |
|----------|--------|----------|
| CPU 对拍 | 孪生路径 I/O 自洽 | NPU 字节正确（SetValue 假绿） |
| SIM | 缺 SET 等**结构**假说 | 粘性挂、杀挂污染 |
| NPU 单轮 | 该次能跑通 | 加压不挂 |
| 干净卡 NPU×N + 同 ISA liboqs | 双门禁（暂行） | Encrypt 粘性**充分条件**（`Q-ENCRYPT-STICKY` 仍开） |

杀挂 / timeout 后同 `ASCEND_DEVICE_ID` 连环挂 = **污染**，解释不了干净卡**首次**挂。排粘性须干净卡。

对拍/交叉失败先答假绿三问（Rule）：golden 是否同源？权威交叉是否已跑？跨核写读是否有 Sync？

---

## 5. 证伪教训（勿重踩）

| 已证伪 | 正确替代理解 | 图谱 |
|--------|----------------|------|
| 「同核双 Cube ⇒ 充分挂因」 | 优先查握手断裂 / 不可达 Set | `J-DUAL-CUBE-NOT-SUFFICIENT` retracted |
| 「多加 Host launch ⇒ 充分修复」 | 有效的是消断裂 / 争用 / 半写；把「加次数」当推荐是反模式 | `J-EXTRA-LAUNCH-NOT-FIX` retracted |

实验冲突 → 改推理链（retracted / inactive），**不是**再堆一条现象。

---

## 6. 开新路径检查单（写码前勾选）

开 MIX / 多段 Host 编排前，逐条回答；任一条「否且未论证」→ 先改设计再写码。

1. 无 CrossCore 的前缀能否独立 launch？若 fused，为何、挂压测计划是什么？  
2. flag ⊆ {1,3,4}（或已批准小表）？是否触及 **5/7**？  
3. AIC 任意 Wait 路径上是否调用 SyncAll / IBWait？  
4. 共享 Â/CBD 等是否单写者写满后再握手？多写者是否有全局屏障？  
5. 是否存在两段 MIX 争用同一组 flag？  
6. 业务写出是否 UB+DataCopy？设备常量是否 uint8 表？  
7. 权威是否同 ISA liboqs？  
8. 不挂计划：干净卡 ×30/×100、预算约 180s、排除污染？  
9. 是否整文件 fork 了 `l18` / 旧 Decrypt 深 GATE / frozen 算子树？（禁）  
10. NTT 是否保持 poly-batch / limb 算术 / S1–S3 无 Gather？

---

## 7. 推荐骨架（模式，非刀号）

### 7.1 Encrypt / Encaps 类

```text
Host:  launch prep (AIV-only, 无 CrossCore) → Sync
       launch compute (MIX, flag∈{1,3,4}) → Sync
```

### 7.2 Decrypt 类（多半写面 + NTT/INTT）

```text
Host:  launch prep (AIV unpack) → Sync
       launch ntt_dot (MIX) → Sync
       launch intt_extract (MIX) → Sync
```

### 7.3 Decaps / 往返

Decrypt 子链 + Re-Encrypt + FO：**段边界清晰 + mid-sync**；勿把上一段半成品当本段输入却无屏障。

### 7.4 KeyGen 类（重建默认）

prep（采样/表）与带 Cube 的 NTT/内积/编码 **切开**；KEM 尾（H(ek)/z/拼 dk）可 AIV-only 另 launch。具体刀号与目录 → `graph-tests/keygen-rebuild-ops/`（不进主文）。

次数对照（仅库存，**≠拓扑同构**）：

| 算子 | stable launch | 重建 launch |
|------|---------------|-------------|
| PKE KeyGen | 2 | 3 |
| PKE Encrypt | 2 | 2 |
| PKE Decrypt | 1 | 3 |
| KEM KeyGen | 2 | 4 |
| KEM Encaps | 2 | 2 |
| KEM Decaps | 3 | 5 |

口诀：Encaps/Encrypt=2；Decrypt=3；PKE-KG=3；KEM-KG=4；Decaps=5。

---

## 8. 关闸与未解

| 状态 | 含义 |
|------|------|
| `J-REBUILD-TOPO-PASSES-GATES` | 短 CrossCore 重建路径上，Encaps/Decrypt/KeyGen **已观察到**双门禁可过（细节在 QUEUE） |
| ≠ 晋级 `examples/stable-*` | 须用户 `#交付#` + customspec |
| `Q-ENCRYPT-STICKY` **仍开** | 粘性挂充分条件未钉死；禁把「某刀绿了」当充分条件关闭 |
| `Q-ULT` **仍开** | 总目标持续；本 KB 是可执行子集，不是证明完备 |

---

## 9. 延伸阅读（原理深文 · 非流水账入口）

| 主题 | 路径 |
|------|------|
| NTT 数学契约 | `MLKEM-NTT-实现总结.md` |
| NTT 设备 / Gather 范围 | `MLKEM-NTT-向量与标量实现指南.md` |
| DataCopy / MTE | `ascendc-DataCopy与数据搬运知识库.md` |
| TQue / Pipe | `ascendc-TQue与Pipe框架知识库.md` |
| SIM 507000 / session | `AscendC-CAModel-SIM-funckey与单session约束知识库.md` |
| Encrypt/Decrypt 拓扑案例附录 | `MIX-*-反卡死拓扑技术总结.md`（**以本文决策为准**；文中刀号仅附录） |

**战役运营（非知识）**：`graph-tests/{encrypt,decrypt,keygen}-rebuild-ops/` · `AGENT_HANDOFF.md` · 当日 `qa/`。  
**分算子旧 KB**：`*-cannbot-rebuild-kb.md` 仅为指针，勿双源维护。

---

## 10. 刷新命令

```bash
python3 scripts/check_rg_dag.py --yaml docs/rg-ascendc-engineering.yaml
python3 thirdparty/reasoning-graph-skill/scripts/rg_validate.py \
  --yaml docs/rg-ascendc-engineering.yaml --repo-root .
python3 scripts/rg_viz.py
```
