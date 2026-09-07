# Encrypt 卡死重写 · 专属知识库

> **只服务核心问题**：NPU 上 Encrypt **不再 SynchronizeStream 卡死**，并最终正确。  
> **非目标（本阶段）**：liboqs 数值对齐、性能打满、修旧核补丁叙事。  
> **维护纪律**：短、准、无歧义；失败优先；每刀实验后更新。流水账不进本库。  
> **配套图谱**：[`docs/rg-encrypt-hang-rewrite.yaml`](../rg-encrypt-hang-rewrite.yaml)  
> **计划**：[`docs/plans/2026-09-06-Encrypt重写工作计划.md`](../plans/2026-09-06-Encrypt重写工作计划.md)

---

## 0. 角色与闸门

| 角色 | 做 | 不做 |
|------|----|------|
| 主控 | 定实验、任务书（目标+验收）、审反馈、维护本库+图谱、定下一刀、解读上机编号 | 不写核内/用例实现代码；不傻等过长实验；不钻牛角尖 |
| Subagent | 在**新目录**按任务书编码+SIM；交回结果 | 不定目标、不改冻结树；超时须回报而非空转 |
| 用户 | 上机时说：测了哪个用例 + 看到哪些编号；主控卡死讨论时拍板 | 不填复杂表格 |

- 已有代码（含旧 Encrypt、旧 toy、stable/pass）：**冻结，只读不改**。  
- **可读**旧 Encrypt；**禁止照抄**其实现（问题可能一并复制）。  
- 积木可用：NTT / SHA3 / 内积等**已多次探针无问题**的实现（卡死不在它们本身）。  
- 实验代码落点：`graph-tests/toys/` → 后再 `graph-tests/enc_related/`。  
- **一实验一目录**；坏了回退干净节点再换路。  
- **单刀限时**；KB/图已列失败路线禁止再走；每刀前遍历本库+DAG；SIM 能做的尽量做，不会了再问用户。

---

## 1. 现象（实机）

| ID | 事实 |
|----|------|
| P1 | Host 已打印 launch `l18_l19`（或同构 MIX 核），随后卡在 `aclrtSynchronizeStream`，无 duration。 |
| P2 | 卡点是**该次 kernel 未完成**，不是 Host 打印逻辑本身。 |
| P3 | Encaps / Decaps-E / PKE Encrypt 可进**同一类** MIX 计算核路径。 |
| P4 | 杀挂死后同卡连环挂 = **次生污染**；不能单独解释干净卡首次挂。 |
| P5 | 未强制重建时可能跑到**旧 fused 二进制** → 近乎必挂（工程粘性，不是算法）。 |
| P6 | 实机曾出现 TRACE **全空（0/16）**：更支持 AIV0 未写到首 mark，而非「mark 机制总坏」。 |
| P7 | KeyGen 实机路径未表现同类卡死（对照：问题偏 Encrypt 计算核/编排，非「整卡不能跑 MIX」）。 |

---

## 2. 核内握手（生产路径摘要 · 禁止当抄码蓝本）

只记**契约**，不贴实现：

```
NTT:  AIV SET(1) → AIC WAIT(1)+Cube → SET(3) → AIV WAIT(3)
中间+GATE（生产时序，T02 要验）:
  AIC: NTT 后立刻 WAIT(4)（先占坑）
  AIV: 做体量（内积/at_jp 等）→ 双 AIV SET(4)
  AIC: WAIT(4) 返回 → SET(8)
  AIV: WAIT(8)
INTT: 再次 SET/WAIT(1)/(3)  （禁止改用 5/7）
```

CrossCore 通道习惯：模板 channel 与 `PIPE_MTE2` 一类；flag 字面量 1/3/4/8。

**时序坑（已写入教训）**：toy「对称 GATE（双方齐到再 AIV SET4）」≠ 生产「NTT 后 AIC 已在 WAIT(4)，AIV 做完体量才 SET4」。字面量相同，语义不同 → **T02 已验：生产时序 + 轻体量在 SIM 不挂**。

---

## 3. 失败知识（优先）

| ID | 结论 | 含义 |
|----|------|------|
| X1 | INTT 换 flag **5/7** | SIM 长时间超时/挂 → **永禁** |
| X2 | AIC 仍在 CrossCore Wait 时 `SyncAll` | 互锁/挂 → **永禁** |
| X3 | 自造 AIC↔AIV SoftSync / softSyncGm / 乱 recreate stream | 挂 → **永禁**；AIV↔AIV 汇合若需要，只跟已验证 Decrypt SoftSyncArrive 定式 |
| X4 | 「拆双 Cube / 多加 Host launch」当充分修复 | **不充分** |
| X5 | 「只怪 l18 第二段」 | 漏查 prep_ntt 等 → **定位失败模式** |
| X6 | 轻量 toy SIM 多 launch / 2-launch **未挂** | **不能**据此宣称生产/NPU 已解 |
| X7 | 照抄现有 Encrypt | 可能把死锁/粘性一并拷入 → **禁抄码，只可读** |
| X8 | CPU 绿 / CPU 经验 | **不作为**卡死因果证据 |
| X9 | AIC 标量写 TRACE GM | SIM 上可假空 → TRACE 须 DataCopy 类路径 |
| X10 | skip-rebuild / 旧二进制 | 实机「偶发/必挂」先排除工程粘性 |
| X11 | 冷机装 CANN 计入实验墙钟 | 首刀假 BLOCKED；**环境刀与实验刀拆开**；实验默认假定 CANN 已就绪 |
| X12 | CPU 孪生 `TPipe` 未析构又开第二个 VECOUT | abort；每段活结束后析构/作用域隔离 |
| X13 | SIM 缺单个 AIC TRACE（如缺 401）但握手后续号齐全 | **TRACE 可见性噪声**；不单独据此判握手失败 |
| X14 | GATE 段假循环体量×10（SIM≈60s）仍不挂 | **纯 UB 假负载加压 ≠ 逼近卡死**；下一维改多 launch / 真积木体量，勿只加空转轮数 |
| X15 | T01–T07 结构/体量/多launch/真MAC/SAMPLE 全绿 | **当前 SIM 玩具族不足以复现 Encrypt 卡死**；继续同质 toys 收益低；须换战略（NPU 批测 / enc_related 近生产体量 / 用户定范围） |
| X16 | KeyGen 默认每次全量重编；Encrypt/Decaps 默认 SKIP_REBUILD=1 | **工程默认不一致**；KeyGen 常新二进制 ≠ 其不卡死原因；Encrypt 跳过重编可放大「旧 bin 粘性」（X10）误判 |
| X17 | KeyGen MIX FSM = SPLIT→MMAD→PACK（flag 1/2/3）；无 GATE 4/8、无 INTT 二段复用 | 与 Encrypt `l18_l19`「NTT + AIC 空等 AIV 大体量 + GATE4/8 + INTT」不同构；KeyGen 不卡死**不能**证明 MIX/CrossCore 无问题，只说明卡死偏 Encrypt 特有编排 |
| X18 | 用户要一次测齐，勿点滴 | 上机入口 `npu_hang_rewrite_one_trip.sh`；**只收打字 TYPE_BACK**；禁同质 toys |
| X19 | 用户无法回传任何文件 | 反馈通道=聊天打字编号；再要 tar/md/日志即违规 |
| X20 | 实机 N0–N10 曾报「全挂」（2026-09-07） | **先环境/整卡，勿归因 Encrypt** |
| X21 | 用户澄清：N0–N10 **秒失败**；屏上**只有 FAIL**；无 ERROR/ACL/preflight/cmake/npu-smi；910B4；**未见卡号** | (1) 秒退 ≠ SynchronizeStream 卡死（真卡死多为 timeout 124）；(2) 只见 FAIL = 多半跑了 **tee 前旧套件**（日志进文件不刷屏）；(3) 旧套件仍会 `export ASCEND_DEVICE_ID`（stable→1 / toys→3），**不是**「没设就默认 0」——只是没打印；(4) 须 `git pull` 到含 tee/device/why 的提交后 **只跑 N0**，打字回 `device=` + `why=` / 末行关键字 |

---

## 3b. 实验台账（toys）

| 刀 | 目录 | 结果 | 学到 |
|----|------|------|------|
| T01 | `graph-tests/toys/T01-mix-ntt13-handshake/` | **PASS**（CPU+SIM） | 新树脚手架可用；NTT 同构 1/3 握手 SIM 不挂；SIM 可缺 401（X13）；首刀曾因冷装 CANN BLOCKED（X11） |
| T02 | `graph-tests/toys/T02-prod-gate-timing/` | **PASS**（CPU+SIM） | 生产 GATE 时序 SIM 不挂；AIC 先 WAIT(4)+AIV 轻体量后双 SET(4)+SET(8)；SIM 仍可缺 401（X13）；**未**覆盖 INTT |
| T03 | `graph-tests/toys/T03-full-fsm-ntt-gate-intt/` | **PASS**（CPU+SIM） | 单 launch 全 FSM 闭环；INTT **复用 1/3**（未用 5/7）；三段 TRACE 可分；SIM 可缺 401（X13） |
| T04 | `graph-tests/toys/T04-gate-volume-stress/` | **PASS**（CPU+SIM） | 同 T03 FSM；GATE AIV **40 轮**（T03=4）；AIC 先 WAIT4 下 SIM 仍不挂；tick≈299k vs T03 49k；→ **X14** |
| T05 | `graph-tests/toys/T05-multi-launch-rounds/` | **PASS**（CPU+SIM） | Host 2× 串行 launch 全 FSM 不挂；印证 X6；轻量壳 |
| T06 | `graph-tests/toys/T06-gate-real-brick/` | **PASS**（CPU+SIM） | GATE 真 Vec MAC（64×8 Mul/Add/Muls）；非 X14 空转；SIM≈4.4s 不挂 |
| T07 | `graph-tests/toys/T07-sampling-then-fsm/` | **PASS**（CPU+SIM） | SAMPLE stub→全 FSM；SIM≈7.8s 不挂；结构缺口已补 |

---


## 3c. KeyGen 对照（为何 PKE/KEM KeyGen 实机不卡）

| 维 | KeyGen（PKE Alg.13 / KEM Alg.19） | Encrypt（卡死主路径） |
|----|----------------------------------|----------------------|
| Launch | 2：prep **AIV_ONLY** → compute **MIX 1+2** | prep + **融合 `l18_l19` MIX**（NTT+内积+INTT+pack） |
| CrossCore | 单段 NTT：`SPLIT→MMAD→PACK`（1/2/3） | NTT 1/3 + **GATE 4/8** + INTT 再 1/3；中间 AIC **Wait(4) 时空等** AIV 大体量 |
| SoftSync | prep 未见 SoftSync 忙等 | Decrypt 线另有；Encrypt 主粘点在 CrossCore/GATE |
| KEM KeyGen | 基本 = PKE KeyGen 设备链 + 尾段 H(ek)/z（宏 `F203_KEM_KEYGEN_TAIL`） | Encaps 走 Encrypt 计算核 |
| run.sh 重编 | `*_SKIP_REBUILD` **默认 0** → 几乎每次 `rm build` 重编 | Encrypt/Decrypt/Encaps **默认 1** → 有 bin+stamp 则跳过 |

结论：KeyGen 不卡死是**编排更简单、无 Encrypt GATE/双段 NTT-INTT 粘点**；不是「会重编所以不卡」。重编默认差异是工程 inconsistency（X16），诊断 Encrypt 卡死时应 `FORCE_REBUILD=1` 排除 X10。

## 4. 可用积木（非卡死源）

| 积木 | 状态 |
|------|------|
| NTT / INTT 探针与已验路径 | 多次测试无问题；**可引用拼装** |
| SHA3 / sampling 相关已验探针 | 可用 |
| 内积 k4 等已验探针 | 可用 |

卡死主线：MIX **编排 + CrossCore 时序 + 体量下的等待**，不是积木算术本身。

---

## 5. SIM vs NPU

| ID | 事实 |
|----|------|
| S1 | 本阶段主战场 = **SIM 穷尽**；完整 Encrypt SIM 慢 → 先骨架 toy。 |
| S2 | Toy 须呈现：sampling → 代数（NTT/INTT 等）；可少算；不对齐 liboqs。 |
| S3 | Toy 要证：2/3 launch 等形态下**多轮跑完不挂**。 |
| S4 | SIM 绿 ≠ NPU 绿；上机只留给「SIM 无法下结论」的刀。 |
| S5 | SIM `aclrtLaunchKernel` 507000 / 多 so / 缺 Sync 等是**平台坑**，与 CrossCore 死等要分开记账（见平台 notes，不展开）。 |

---

## 6. TRACE 编号约定（上机友好）

用户反馈形态：**用例名 + 编号序列**（如 `toys/T01 … 101 102 201`）。无长 log。

| 段 | 范围 | 谁打 |
|----|------|------|
| Host | 100–199 | Host：进 main、launch 前、Sync 返回、退出 |
| AIV0 | 200–299 | 设备 |
| AIV1 | 300–399 | 设备 |
| AIC | 400–499 | 设备 |
| 保留/异常 | 900–999 | 断言失败/早退 |

规则：

- 打印**纯十进制整数**一行一个（或空格分隔），勿长字符串。  
- **每个实验目录**自带 `trace_map.md`：编号 → 文件:行含义 → 「出现/缺失」解释。  
- 缺号 = 未到达该点（比「卡住」更可推理）。  
- 主控凭 map 解读；用户不必填表。

---

## 7. 当前推理焦点（卡死）

开放问题（进图谱）：

1. 空 TRACE / 挂死时，卡在 **哪一次** Wait/Set？  
2. 生产时序下「AIC 先 Wait(4) + AIV 大体量」是否为必要复现条件？  
3. 哪些编排在 SIM 加压可逼近粘性，哪些只能 NPU 见？  

已知边界：积木 OK；禁 5/7、禁 Wait 中 SyncAll、禁自造 SoftSync；禁抄旧 Encrypt；轻量 toy SIM 未挂 ≠ 已解（X6）。  
已闭合（局部）：T01–T07（握手/GATE/全FSM/假循环×10/2×launch/真Vec MAC/SAMPLE前置）**SIM 全绿**。
未闭合（卡死复现）：SIM 玩具线**未能**逼出 SynchronizeStream 挂；剩余主因候选在 **NPU / 生产体量与真实积木编排**（S4）。
**主控闸门**：SIM 骨架维已穷尽 → **停派 toys**。NPU 一次测套件已备（X18/X19）；**实机反馈：N0–N10 全挂（X20）** → 先环境；下一刀只跑 N0 + 打字首错（禁要文件、禁同质 toys）。  
入口：`bash scripts/npu_hang_rewrite_one_trip.sh` · 说明：`docs/engineering/Encrypt卡死重写-实机一次测清单.md`。
