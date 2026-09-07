# ENCRYPT-GAP：套件未覆盖结构差 → 离线 toys → 仅剩上机项

**状态**：2026-09-07 主控推理锁定（BRANCHING **B3** + N7–N10）。  
**工作法**：父 Agent 列差 / 定实验 / 刷图谱；**subagent 编码**；SIM 穷尽后再谈上机。  
**禁**：把「想到一个 NPU 刀」当进度；复踩 retracted（仅 GATE / 仅 stub flag1/3 / 仅双 Cube…）。

---

## 1. 结构差清单（只读对照 · 禁抄码）

对照面：

| 侧 | 路径 |
|----|------|
| 套件已覆盖 | `graph_tests/toys/toy-e01` … `e15` + `npu_suite` C0–C2 |
| Encrypt 挂面（只读） | `examples/stable/.../kem-encaps-k4/compute/f203_encrypt_l18_l19_kernel.cpp` FSM 注释与 CrossCore 序 |

| ID | 结构差 | 套件（E01–E15） | l18 挂面 | 离线可证？ | 备注 |
|----|--------|-----------------|----------|------------|------|
| G1 | **INTT CrossCore flag** | NTT **1/2**；INTT **5/6 独立**；壳 SET(**4**) | NTT **1/3**；GATE 后 INTT **复用 1/3**（禁用 2 作 CrossCore） | **是**（孪生 toys） | 与 cannbot「flag 复用节奏 / Matmul 保留区」相关；**禁**单独复测「stub 1/3」 |
| G2 | **GATE 语义** | flag **4** = L2 入口「双 AIV→AIC 齐步」 | **4**=内积齐；**8**=AIC 放行 INTT（`ST_AT_JP_GATE`） | **是**（与 G1 同核序） | 「仅 GATE」已 retracted；须嵌在 NTT→GATE→INTT 全序 |
| G3 | **单 MIX 内 NTT→内积→GATE→INTT** | 真链在 L2，无 Tag5T 式 at_jp pad8→k8 INTT 编排 | 同 launch：NTT → at_jp → SET4 → Wait8 → INTT | **部分**（stub 可仿序；真 at_jp 禁抄） | 粘性充分条件未知；先 stub 序 |
| G4 | **Host launch 面** | 套件 2-launch + Host `1xx` | Encaps 多 launch；挂在已 print `l18_l19` 后 Sync 未回（N9） | **否**（粘性） | SIM 不粘（`J-sim-not-sticky`） |
| G5 | **冷启动×多轮** | 套件未做 R×N 粘性结论 | Encaps PASS×10 后 r11 挂（N10） | **否** | 必须上机 |
| G6 | **污染后首轮挂** | 无 | 同日早波可 r1 挂（N10） | **否** | 必须上机；与 G5 对照 |
| G7 | **设备 TRACE 槽** | 套件三位 printf | l18 有 `FusedTraceMark`，默认 Host 不传；须 `F203_L18_TRACE=1` | **否**（钉 Wait 段） | 上机开 TRACE；SIM 可先核 Host 传参路径（轻） |
| G8 | **Decaps Phase-D** | 套件不覆盖 | 可挂 `f203_decrypt_device_fused`（N8） | 另线 | 本文件主线仍是 Encrypt/l18；Decaps 不混刀 |

**假说排序（禁充分条件复踩）**：

| H | 内容 | 离线 toys | 上机 |
|---|------|-----------|------|
| H-reuse | **G1+G2 组合**：NTT→GATE4/8→INTT **复用 1/3** 是套件未覆盖且靠近挂面的同步形 | E17 vs E18 | NPU-E17/E18 |
| H-sticky-suite | 套件结构本身多轮也会粘 | — | NPU-RxN-C2 |
| H-l18-stage | 挂死在 NTT / GATE4 / INTT 某一 Wait | Host 传 TRACE 路径检查 | NPU-TRACE |
| H-pollute | 卡污染改变首挂轮次 | — | NPU-CLEAN vs DIRTY |

---

## 2. 离线 toys 矩阵（须做完）

| Toy | 单因子 | 形态（stub，禁抄 Encrypt 业务） | 验收 | 图谱 |
|-----|--------|--------------------------------|------|------|
| **E17** `toy-e17-l18-fsm-reuse13` | l18 **同步序 + INTT 复用 1/3** | 单 launch MIX：`Wait/Set(1,3)` 伪 NTT → 双 AIV `SET(4)` / AIC `Wait(4)+SET(8)` → 再 `1/3` 伪 INTT；Host `1xx`；≥8 轮 SIM | **SIM ✅** + sync_audit（SYNC-03 假阳性） | `D-exp-e17` support |
| **E18** `toy-e18-l18-fsm-sep56` | **仅改** INTT 通道为 **5/6** | 与 E17 同序；GATE 后 INTT 用 5/6 | **SIM ✅** + sync_audit | `D-exp-e18` support |
| **E17-audit** | 静态 | cannbot `sync_audit.py` + deadlock-triage 笔记 | ✅ `/opt/cursor/artifacts/e17-sync-audit.json` | `J-use-cannbot-on-gap` |

**不做（本波）**：真 NTT/INTT/at_jp；ByteDecode；正确性 golden；OMIT_SET4 复测；「仅 GATE」空壳。

**SIM 期望**：两者都绿（`J-sim-not-sticky`）。离线目标是 **结构可活 + 审计无真死等红线**，不是复现粘性。

---

## 3. 仅剩必须上机项（授权后再跑）

> 下列在 E17/E18 SIM+审计完成后才排队。用户未授权 → **不连 NPU**。

### NPU-1 · 套件 R×N（C2×7）

| 项 | 内容 |
|----|------|
| **测什么** | 套件最高档 C2 多轮是否粘性（B3-sticky vs B3-clean） |
| **怎么跑** | `bash graph_tests/npu_suite/run_rxn_npu.sh`（默认 C2×7）；`TIMEOUT` 紧；FORCE 首轮可放宽 |
| **期望反馈** | 每轮 `REPORT:` 是否 `111`；若挂：Host 最后三位 + 是否已 print L2 launch |
| **反馈→计划** | **仍绿** → 加强「挂因 ∉ 套件覆盖」→ 开 **NPU-2**；**第 N 轮挂** → 定最低 N，查轮间状态，**暂缓** E17/E18 上机，先轮间假说；**首轮挂** → 对照环境/二进制（N4），FORCE 重建复测 |

### NPU-2 · E17 vs E18 同机对照（单因子上机）

| 项 | 内容 |
|----|------|
| **测什么** | 在 910B3 上，**复用 1/3** vs **分离 5/6** 是否改变粘性（支持/削弱 H-reuse） |
| **怎么跑** | 干净卡；各 toy `TOY_ROUNDS≥11`（对齐 N10 r11）；串行；短超时截断 |
| **期望反馈** | E17/E18 各自：PASS×k 或 HANG@轮次 + Host `1xx` |
| **反馈→计划** | **E17 挂、E18 绿** → H-reuse **support** → 重写路径默认 INTT 独立 flag（仍禁抄 l18）；**两者都挂** → reuse **非充分** → 开 NPU-3 / 加深 G3；**两者都绿** → stub 序不足，上机转 NPU-3（真业务挂面+TRACE）；**E18 挂、E17 绿** → 异常，查实现/构建偏置 |

### NPU-3 · Encaps `l18_l19` + `F203_L18_TRACE=1`

| 项 | 内容 |
|----|------|
| **测什么** | 钉挂死落在 TRACE 哪一段（NTT / IP / GATE / INTT） |
| **怎么跑** | `hang_observe` 或等价；**必须**开 TRACE；冷启动×多轮；TIMEOUT=240 |
| **期望反馈** | Host：launch 文案 + RUNTIME 3min 与否；设备槽位最后非零（或全空→EARLY） |
| **反馈→计划** | 停在 GATE 前 → 查 SET(4) 可达；停在 GATE 后 INTT Wait(1) → 对齐 H-reuse / NPU-2；槽全空 → EARLY；与 Decaps 混挂 → 分刀，不合并根因 |

### NPU-4 · 干净卡 vs 污染（N10 对照封口）

| 项 | 内容 |
|----|------|
| **测什么** | 首挂轮次分布是否依赖污染 |
| **期望反馈** | 干净：PASS×≥10 后挂 or 不挂；污染：r1 是否可挂 |
| **反馈→计划** | 两现象并存已观测 → 封口为事实；根因刀仍走 NPU-2/3，**不**把「先跑 KeyGen 清洗」当修复 |

---

## 4. 主控关门条件（本波离线）

- [x] §1 差清单入库（本文件）
- [x] E17 SIM ≥8 轮绿 + sync_audit 笔记（`FEEDBACK-E17`；wall ~81s）
- [x] E18 SIM ≥8 轮绿 + sync_audit 笔记（`FEEDBACK-E18`；wall ~101s）
- [x] 知识库 §6 / 图谱改为「离线 GAP toys 完成 → **待授权**上机 NPU-1…4」
- [x] **不**主动要求用户开机

**离线结论（更新）**：G1+G2 stub 在 SIM **与实机×11** 均可活 → **H-reuse 非充分**（N12）。  
**上机 NPU-1…4 已跑**：见知识库 N11–N14；关键 = N13 **空 TRACE** → 下一刀 **EARLY**（离线优先）。  
**请控制台关机。**

---

## 5. 指针

| 路径 | 用途 |
|------|------|
| `BRANCHING.md` §B3 / ENCRYPT-GAP | 分支树 |
| `docs/notes/Encrypt-实机无卡死-知识库.md` | 领域事实 |
| `docs/rg-encrypt-npu-hangfree.yaml` | 图谱 |
| `thirdparty/cannbot-skills/ops/ascendc-sync-audit/` | 审计 |
| `SUBAGENT_RULES.md` | 单 subagent / 时限 |
