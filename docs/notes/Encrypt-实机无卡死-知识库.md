# Encrypt 实机无卡死 — 专属知识库（v2 · cannbot 刷新）

**焦点**：Encrypt **实机无卡死且正确**；当前卡在 **粘性挂根因未钉死**。  
**配套图谱**：[`docs/rg-encrypt-npu-hangfree.yaml`](../rg-encrypt-npu-hangfree.yaml)  
**计划**：[`graph_tests/ENCRYPT_REWRITE_PLAN.md`](../../graph_tests/ENCRYPT_REWRITE_PLAN.md)  
**本版**：2026-09-07 **从头刷新**——领域证据保留，**工程层以 cannbot-skills 为准**；此后实验/写码必须 **图谱 + cannbot** 双开。

| 层 | 权威 |
|----|------|
| 领域 / 挂因 / 下一刀 | 本库 + 图谱 + `BRANCHING` |
| AscendC 同步 / 卡死 / API / 环境 | **`thirdparty/cannbot-skills`**（就地读用） |
| 本仓 KernelLaunch / CPU·SIM | `ascendc-engineering-notes` |

---

## 0. 工作法（硬锁）

| 规则 | 说明 |
|------|------|
| **图谱驱动** | 每刀前遍历图谱 active 节点；每刀后回写 F/J/D |
| **cannbot 驱动工程** | 写/改 MIX·CrossCore 前读 `ascendc-sync-audit`；卡死取证用 `ascendc-crash-debug`；API 用 `ascendc-api-best-practices` |
| **禁抄 Encrypt** | 可读冻结实现；禁止当模板搬码 |
| **一实验一目录** | `graph_tests/toys/`（及后续 enc_related） |
| **短刀** | 单刀墙钟紧；超时止损 |
| **禁复踩** | 图谱 `retracted` 充分条件禁止再开 |
| **SIM→NPU** | SIM 穷尽结构后再上机；**910B3 须用户授权** |
| **反馈** | 用户只打字；禁回传文件；NPU 挂因只认 Host `1xx` |
| **正确性后置** | ByteDecode / KAT：挂因未明前不做 |

### 0.1 cannbot 使用入口

| 场景 | Skill 路径（均在 `thirdparty/cannbot-skills/ops/`） |
|------|-----------------------------------------------------|
| 同步 / 死等 / hang 嫌疑 | `ascendc-sync-audit/`（`workflows/deadlock-triage.md` + `scripts/sync_audit.py`） |
| 卡死 vs 崩溃 / plog | `ascendc-crash-debug/` |
| CrossCore / Pipe / DataCopy | `ascendc-api-best-practices/`（`references/api-crosscore-sync.md` 等） |
| 环境 / 架构 | `ascendc-env-check/` · `npu-arch/` · `cann-env-setup/` |
| SIM 流水 | `ops-simulator/` |
| 精度（挂因之后） | `ascendc-precision-debug/` |

**审计纪律（来自 cannbot）**：脚本红线/高优先级候选须呈报，禁止 LLM 凭感觉否决；MIX 单翻译单元内 `ASCEND_IS_AIC/AIV` 分支易触发 **SYNC-03「同侧」假阳性**——须人工对照分支，不得据此改绿核「消警告」。

---

## 1. 拼装铁律

| 可 | 不可 |
|----|------|
| 用已绿积木（NTT/SHA3/内积/压码…）组合 | 把「积木绿」当成「Encrypt 编排已正确」 |
| cannbot 审计后按图谱开单因子刀 | 同目录叠改；零散未授权上机 |
| Host 折 μ（设备禁 PrefixEmbedMu） | AIC 仍 Wait 时 SyncAll；自造 SoftSync 协议 |

---

## 2. 实机硬事实

| ID | 断言 | 来源 |
|----|------|------|
| N1 | Encaps/PKE Encrypt 可挂在 Host 已 launch `l18_l19`（或 prep）且无 Sync 回 | 历史短报 |
| N2 | 挂时设备 TRACE 常空；NPU 上 `AscendC::printf` 常不进 host → **挂因只认 Host 1xx** | 历史 + 本分支 Host-only |
| N3 | KeyGen 未同类粘性挂 → 偏 Encrypt 特有编排（非「MIX 不能跑」） | 对照 |
| N4 | 旧二进制 / SKIP_REBUILD 可混淆现象，**非**根因 | 构建策略 |
| N5 | Host 文案在 ACL launch 前打印 → 「卡在文案」= Sync 未回 | 观测 |
| N6 | 历史多轮（约第 7 轮）可挂 | 2026-09-04 |
| N7 | **2026-09-07**：NPU_SUITE 单轮 C0/C1/C2 均 `PASS last=111` | 用户打字 |

### 2.1 KeyGen vs Encrypt（结构差 · 可证伪）

| | KeyGen | Encrypt / Encaps |
|--|--------|------------------|
| CrossCore 主路径 | 多为 1/2/3（split/mmad/pack） | NTT/INTT 1/3 + **GATE Wait(4)/SET(8)** 等 |
| SampleNTT | 常独立 AIV launch | Encaps L1 可把 SampleNTT **嵌进 MIX**（嫌疑） |
| 重编默认 | 常 SKIP=0 | Encrypt/Encaps 常 SKIP=1（对照须统一 FORCE） |

假设 **H-K1…H-K5**（GATE/at_jp、SampleNTT∈MIX、flag 复用、负载、构建偏置）仍待 NPU 证伪；**不得**把「KeyGen 会重编」当成不挂原因。

---

## 3. 失败与禁令（失败优先）

### 3.1 领域已证伪 / 已回退（禁复踩）

| | 结论 |
|--|------|
| ✗ | 仅第二段 l18 / 仅双 Cube / 仅 Host launch 次数 / 仅 GATE / stub 仅 flag1/3 为粘性充分条件 |
| ✗ | AIC Wait 中 SyncAll；自造 SoftSync 双向汇合 |
| ✗ | Host 折 μ SIM 绿 ⇒ 可零散上机结案 |
| △ | 缺 SET(4)⇒SIM 124：**真**且有用，≠ 实机粘性挂 |
| △ | SIM 不能复现粘性挂（`J-sim-not-sticky`） |

### 3.2 cannbot 工程禁令（写码必守）

摘自 sync-audit / api-crosscore-sync（完整见 skill references）：

| ID | 禁令 |
|----|------|
| C-SYNC-01 | Wait 先于 Set → 死等 |
| C-SYNC-03/04 | CrossCore Set/Wait 必须路径可达且配对；提前 return 跳过 Set → 死等 |
| C-SYNC-07 | flagId 0–15；**禁止**手写 CrossCore 与 Matmul 高阶 API 混用同 flag |
| C-SYNC-08 | 所有到 Wait 的路径必须 Set |
| C-SYNC-12 | SyncAll 全核不对称 → 部分核死等；**禁止** AIC 仍 CrossCore Wait 时乱加 SyncAll |
| C-PIPE | 跨 PIPE 依赖勿用单 `PipeBarrier<PIPE_V>` 冒充；异步搬出后复用 buffer 前注意完成关系 |
| C-AUDIT | 审计候选须人工确认；**SYNC-03 同侧**在单文件 MIX 上对 E01/E13/E15/l18 **均报警** → 默认当假阳性候选，非自动改码依据 |

### 3.3 本分支 cannbot 审计快照（2026-09-07）

| 目标 | 非性能高优 | 解读 |
|------|------------|------|
| toy-e01 / e13 / e15 `mmad_custom.cpp` | SYNC-03「同侧」×1 + 大量 SYNC-09（PIPE_ALL） | 单文件 MIX 分支；**未**据此判核错误；PIPE_ALL 为性能项 |
| stable pke-encrypt `f203_encrypt_l18_l19_kernel.cpp`（只读） | 同型 SYNC-03×1 + SYNC-09 | 同上；只读对照，**不抄码** |

产物：`/opt/cursor/artifacts/sync-audit-e*.json`（本地审计日志，不进 git）。

---

## 4. 可用积木与套件（卡死不在积木本身）

| 层 | 状态 |
|----|------|
| E01–E03 | 2-launch+SET4 / SoftSync 可选 / 阶段齐 · SIM ✅ |
| E04–E12 | 真 NTT→…→k=2 · SIM ✅ |
| E13–E15 | 形态粘合 → Â 2×2 SampleNTT（独立 launch）· SIM ✅ |
| E16 / NPU_SUITE | C0/C1/C2；**NPU 单轮全绿 N7** ✅ |
| SampleNTT | **须独立 launch/phase**（E14 教训；禁嵌同一 MIX 真链抢 TPipe） |

握手不变量：L2 若 AIC `Wait(4)`，双 AIV **必须可达** `SET(4)`。

---

## 5. TRACE（挂因）

| 段 | 谁 | 要点 |
|----|----|------|
| 1xx | Host | **NPU 挂因唯一可靠面**：`100/101/110/111`；C2 另有 `104/106` |
| 2xx–7xx | 设备 | SIM 有用；NPU 常不可见 |
| 成功 | | Host **`111`** |
| 主挂征象 | | 有 **`110`** 无 **`111`** |

套件：`graph_tests/npu_suite/`；分支树：`BRANCHING.md`（B0–B3）。

---

## 6. 目标分解与当前刀

```
积木 SIM 绿 → NPU 单轮套件绿(N7) → [多轮粘性 R×N | ENCRYPT-GAP]
  → 钉 Q-root-cause → 正确性（ByteDecode/KAT）→ 实机无卡死且正确
```

| 状态 | 内容 |
|------|------|
| **已证** | 套件覆盖的 2-launch+SET4+粘合+Â2×2 **单轮**不粘性挂（N7 / B3） |
| **待证** | 多轮粘性（R×N C2×7）；或套件未覆盖的 Encrypt 特有结构（ENCRYPT-GAP） |
| **下一刀** | ① 用户授权上机 → R×N；② 无卡时 → 图谱指导下 ENCRYPT-GAP **单因子**短刀（写码前 cannbot audit） |
| **缓** | ByteDecode / 权威交叉 / 更高 k |

---

## 7. 指针

| 路径 | 用途 |
|------|------|
| `thirdparty/cannbot-skills/ops/ascendc-sync-audit/` | 同步审计 |
| `thirdparty/cannbot-skills/ops/ascendc-crash-debug/` | 卡死调试 |
| `graph_tests/npu_suite/` | 上机套件 |
| `docs/rg-encrypt-npu-hangfree.yaml` | 推理 DAG |
| 冻结 skel / clean | 只读判决；出门不带码 |

---

## 变更

| 日期 | 内容 |
|------|------|
| 2026-09-07 | **v2 从头刷新**：cannbot 工程层 + 审计快照；压缩历史台账；锁定「图谱+cannbot」双开写码 |
| （v1 摘要） | E01–E16 SIM；N7 单轮全绿；Host-only；正确性后置 |
