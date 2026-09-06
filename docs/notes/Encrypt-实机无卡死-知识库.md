# Encrypt 实机无卡死 — 专属知识库

**焦点**：最终目标 = Encrypt **实机无卡死且正确**；当前问题 = **会卡死**。  
**配套图谱**：[`docs/rg-encrypt-npu-hangfree.yaml`](../rg-encrypt-npu-hangfree.yaml)  
**计划**：[`graph_tests/ENCRYPT_REWRITE_PLAN.md`](../../graph_tests/ENCRYPT_REWRITE_PLAN.md)  
**维护**：主控；短、准、常改；**失败 ≥ 成功**；禁流水账。  
**旧材料**：`docs/rg-kem-encrypt-hang.yaml` 等只作参考，不强制合入。

---

## 0. 角色（问题 7 定稿）

| 角色 | 职责 | 边界 |
|------|------|------|
| **主控** | 拥有目标与问题；维护本库+DAG；设计实验与验收；派发/回收；据结果选下一刀；决定何时上机 | 不做实验编码实现 |
| **Subagent** | 按目标+验收执行实验（实现、跑测、回报） | 不定方向、不改图谱/本库 |

各写本职所需的一切产物；**按职责分工，不按文件后缀分工**。  
**已有代码全冻结**：只读参考，禁止改；新实验新目录。

---

## 0.1 实验节奏（2026-09-06 用户加锁）

| 规则 | 说明 |
|------|------|
| **单实验短** | 派给 subagent 的单次实验必须短；墙钟建议 ≤30–40min；超时先停，判「任务过大 / 路线有坑」，禁止傻等 |
| **主控同样** | 禁止钻牛角尖；图谱已 `retracted` 的充分条件路线 **禁止再开对照实验** |
| **每刀前必刷** | 安排下一实验前：沉淀本库 → 遍历图谱 → 再派单 |
| **能做尽做** | SIM 能做的尽量做；**真不知下一步**才停下来与用户讨论，禁止闷头乱撞 |
| **禁复踩清单** | 见 §3 全部 ✗ 行；尤其勿再测「仅双 Cube / 仅 GATE / 仅 launch 次数 / stub 仅 flag1/3」等 |

**层1握手**：缺 SET⇒124、SET 可达⇒绿 —— 已由冻结 `fix-encrypt-skel-*` 证实 → **采纳为砖，不重做发现实验**。新实验只做**新问题**（组合砖、多轮 Host、数字 TRACE、2-launch 骨架等）。

---

## 1. 拼装铁律

| 可 | 不可 |
|----|------|
| 读一切（含现 Encrypt/文档/旧图） | **照抄**现 Encrypt 实现（问题会一起抄进来） |
| 用已多次绿的 **NTT / SHA3 / 内积** 作积木 | 把积木的“能跑”当成 Encrypt 编排已正确 |
| 经实验+图谱自底向上重拼 | 同目录叠改多实验；零散请上机 |

---

## 2. 实机观测（硬事实）

| ID | 断言 |
|----|------|
| N1 | Encaps/PKE Encrypt 可挂在 Host 已打印 `launch … l18_l19`（或 `prep_ntt` / split `ntt_y`）且无对应 duration |
| N2 | 挂在 l18 时设备 TRACE 常 **全空** → AIV 早期标记未落盘可见 |
| N3 | KeyGen **未**复现同类粘性挂 → 触发在 Encrypt 特有编排/交互（详见 §2.1） |
| N4 | 删 out* / 不 FORCE 复用旧二进制：可混淆现象，**不是**根因（详见 §2.2） |
| N5 | Host 文案在 ACL launch **之前**打印 → “卡在 launch 文案”= Sync 未回，非文案本身 |
| N6 | 2026-09-04：PKE Encrypt 第7轮仍挂 l18（当时路径仍可能含设备侧 μ 相关逻辑）；Hostμ 热修 **SIM 绿 ≠ 已证 NPU 不挂** |

### 2.1 KeyGen 为何不挂（结构对照 · 2026-09-06）

稳定树：`examples/stable/ml-kem/ml-kem-1024/`。Host 均为约 **2 launch**，故 **不是**「launch 次数少所以不挂」。

| 维度 | PKE/KEM KeyGen | PKE Encrypt | KEM Encaps |
|------|----------------|-------------|------------|
| L1 | `keygen_prep` **AIV_ONLY** | `encrypt_prep` **AIV_ONLY** | `prep_ntt` **MIX**（头+CBD+**NTT 同核**） |
| L2 / 计算核 | `mmad_custom` MIX；CrossCore 主用 **1/2/3**（split/mmad/pack） | `l18_l19`：同核 **NTT + at_jp + INTT**；**Wait(4)/SET(8)** GATE | `l18_l19`（常 `ySrc=null` skipNtt）仍含 **at_jp + INTT + GATE 4/8**；L1 另有 SoftSync→GATE |
| SampleNTT | **独立 AIV launch**，不进 MIX | 独立 AIV | **嵌在 MIX `prep_ntt`**（与知识库禁令同向嫌疑） |
| 历史挂点文案 | 无 `l18` / `prep_ntt` | 常停在 `l18_l19` | `prep_ntt` **或** `l18` 均可 |

**更像根因方向的结构差（可证伪）**

| ID | 假设 | 证伪/加固 |
|----|------|-----------|
| H-K1 | 粘性挂依赖 Encrypt 特有 **Wait(4)/GATE(8)/at_jp**；KeyGen 无此 FSM | NPU TRACE：挂时停在 4xx↔5xx GATE 边界；KeyGen 同卡多轮不应出现该号段 |
| H-K2 | **SampleNTT∈MIX**（Encaps L1）是因子 | `SPLIT_PREP` / Â 独立 launch 对照；只看挂/不挂 |
| H-K3 | L2 上 **flag 1/3 先 NTT 再 INTT 复用** + at_jp 同 launch | PKE（双 Cube 一 launch）vs Encaps skipNtt；两者都挂则非「仅双 Cube」 |
| H-K4 | 「KeyGen 更轻」 | **弱**：KeyGen 仍有大量 SampleNTT+Tag5；若墙钟≥Encaps 仍不挂 → 否证负载主因 |
| H-K5 | KeyGen「稳」来自每次干净构建（§2.2） | **对照偏置**；统一 FORCE/SKIP 后 KeyGen 仍应不粘性（若仍不挂则 H-K5 不足以解释） |

**注意**：Decrypt 设备侧也有 SoftSync + GATE 4/8（`pke-decrypt` fused）。若 Decrypt **也不**粘性挂，则「有 GATE4/8」不是充分条件，需叠加 **Encrypt/Encaps 特有组合**（at_jp 与 NTT/INTT 同 launch、SampleNTT∈MIX、多轮 ws 等）。待实机/对照补强。

### 2.2 编译：KeyGen 总重编 vs Encaps 跳过（2026-09-06）

| 用例 | 默认 `*_SKIP_REBUILD` | 行为 |
|------|----------------------|------|
| PKE KeyGen | **0**（默认关跳过） | 未显式 SKIP=1 时 **`rm -rf build` + 全量 cmake**（`…/pke-keygen-k4/run.sh`） |
| KEM KeyGen | **0**（`KEM_SKIP_REBUILD` 默认 0） | 默认走进 build；且每次 build 前 **重写** `prep/alg7/*.h` ROM → 易触达全量重编 |
| PKE Encrypt / KEM Encaps / Decaps | **1** | stamp + 二进制命中则 **跳过 cmake** |

**结论**

1. 这是 **run.sh 策略不对称**，不是「Encaps 神秘跳过、KeyGen 源总变」的玄学；也不是挂因本体。  
2. **风险 A（KeyGen）**：每次干净树 → 「KeyGen 从不挂」的对照里混入 **干净 ABI** 偏好；应用 `KEYGEN_SKIP_REBUILD=1` / 与 Encaps 同策略再压多轮，排除 H-K5。  
3. **风险 B（Encaps/Encrypt）**：默认跳过 → 改设备码未 `FORCE_REBUILD=1` 可能跑 **陈旧 so**，造成挂/不挂假抖动。N4 已记：旧二进制可混淆。但 qa 显示 **FORCE 后 Encaps 仍粘性挂** → **陈旧二进制不是粘性根因**。  
4. **纪律**：挂因对照实验前 **统一 FORCE 或统一 SKIP**，禁止用「一个总重编、一个总跳过」当结构差。

---

## 3. 失败知识（优先于成功）

| 已证伪 / 已回退 | 含义 |
|-----------------|------|
| ✗ 仅第二段 l18 充分 | prep_ntt / ntt_y 也可挂 |
| ✗ 仅同核双 Cube 充分 | 拆/加压 Cube 未消粘性 |
| ✗ Host launch 次数决定 | split 仍挂 |
| ✗ 仅 GATE / prep∈MIX 充分 | stub+合法 GATE SIM 仍绿 |
| ✗ stub 下仅 MIX+flag1/3 充分挂 | skel toy SIM 绿 |
| ✗ stub 更大/更多 Cube 充分挂 | HEAVY SIM 绿 |
| ✗ AIC Wait 中 SyncAll | 已禁；合法时机仅 AIC 已返回后 |
| ✗ 自造 SoftSync 双向汇合 | 回退；若汇合只跟 Decrypt SoftSyncArrive 定式 |
| ✗ 滥增 Host launch / 标量碎写 GM | 禁当推荐解（可作负面对照） |
| ✗ Host折μ SIM绿 ⇒ 可零散上机验证 | 用户纠偏：SIM 穷尽前不上机 |
| △ 缺 SET(4)⇒SIM 124 | **真**且有用，但是 **SIM 可造的挂**；**不能**等同实机粘性挂 |
| △ 空 TRACE ⇒ AIC Wait(4) 等 AIV SET | SIM 相容解释；实机是否同构 **未钉死** |
| △ clean P0/P1a SIM 绿 | 结构约束可 SIM 存活；**未**证明 NPU 不挂 |

---

## 3.1 暂缓（用户锁 · 2026-09-06）

| 暂缓项 | 理由 |
|--------|------|
| t̂·ByteDecode | 落在卡死点**之后**；对找挂因无帮助 |
| 权威交叉 / KAT 正确性比对 | 同样属正确性；**找出卡死原因前不做** |
| 更高 k 扩几何 | 非挂因主路径；墙钟已紧 |

**当前主问**：`Q-root-cause`（实机粘性挂可操作根因）。SIM 已表明不能复现粘性挂 → 取证靠 **NPU_SUITE + 数字 TRACE**。

## 4. 仍成立的约束（写新 toy 必须遵守）

1. **最终验收 = NPU**；迭代关键 = **SIM**；不沉淀 CPU 经验。  
2. skipNtt / L2 入口若 AIC `Wait(4)`：双 AIV **必须可达** `SET(4)`（缺则 SIM 必挂）。  
3. **μ 默认 Host 折**；设备 skipNtt **禁止** PrefixEmbedMu（结构默认，非热修旁路思维）。  
4. 禁止 AIC 仍 Wait 时 SyncAll；AIV 汇合跟既有 SoftSyncArrive，不自造协议。  
5. CrossCore：NTT/INTT 常用 flag **1/3**；l18 at_jp 路径另有 GATE **4/8**。  
6. SIM 可独立下结论的实验必须先做完；上机只留 **整份 NPU_SUITE** + **数字 TRACE**。  
7. 一实验一目录：`graph_tests/toys/` → 以后 `graph_tests/enc_related/`。

---

## 5. 可用积木（卡死不在它们本身）

| 积木 | 用法 |
|------|------|
| NTT / INTT（活跃 vec 探针路线） | 代数阶段积木；勿从 frozen 抄 |
| SHA3 / SHAKE | sampling/哈希积木；toy 可 stub 降算量 |
| CBD(η=2) | 采样积木；E08 已接短链 |
| 内积 / basemul（Alg.11/12 自包含） | L2 计算积木；E06 已接 |
| Compress_d（d=4 向量 Barrett） | 压系积木；E09 已接；≠整图 Encrypt tail |
| ByteEncode_d（d=4） | 编码积木；E10 已接；只读 `pass-f203-byteencode-d-vec-k4` |
| Decompress_d（d=1，μ 嵌入） | 消息积木；E11 已接；只读 `pass-f203-decompress-d-vec-k4` |
| SampleNTT（Alg.7） | Â 采样积木；E15 已接完整 2×2（独立 launch）；只读 `pass-fix-f203-alg7-sample-ntt-k2` |
| **SampleNTT 编排约束** | **禁止**把 SampleNTT 嵌进 MIX 真链同 kernel（易 TPipe 互抢 SIM 挂）；E14：独立 launch（phase）再回粘合 |
| SoftSyncArrive 定式、CrossCore 1/3·4/8 | 同步积木；按约束用，不发明 |

---

## 6. TRACE 协议（主控定稿）

**打印**：只输出 **三位十进制**（例 `501`），禁止长字符串。  
**对照表**：每实验目录 `TRACE.md` + 本库下表总册。

| 段 | 谁 | 含义 |
|----|----|------|
| 1xx | Host | launch 前后、Sync 返回 |
| 2xx | L1 AIC | prep/NTT Cube 侧 |
| 3xx | L1 AIV | prep/采样侧 |
| 4xx | L2 AIC | Wait(4)/GATE/INTT Cube |
| 5xx | L2 AIV | SET(4) 前/后、at_jp、pack |
| 9xx | 任一侧 | 失败/超时哨兵 |

**判读**：

- 见 `N` 未见 `N+1` → 卡在 `N`→`N+1` 之间。  
- Host 有 `1xx` 进入 L2、设备无任何 `4xx/5xx` → 核未进或极早死等（对齐历史空 TRACE）。  
- 有 `5xx` SET 前、无 SET 后 → AIV 死在 SET 前工作。  
- 有双 AIV SET 标记、无 AIC 过 Wait → AIC 侧/旗语问题。

**预留号（骨架；实验可增，不复用已发布号）**：

| 号 | 位置 | 出现 | 未出现 |
|----|------|------|--------|
| 100 | Host 将 launch L1 | 进入 L1 | Host 未发 L1 |
| 101 | Host L1 Sync 回 | L1 完成 | 卡在 L1 |
| 110 | Host 将 launch L2 | 进入 L2 | 未发 L2 |
| 111 | Host L2 Sync 回 | **整段成功关键点** | 卡在 L2（历史主挂点） |
| 400 | L2 AIC 入口 | AIC 已进 L2 | AIC 未进 |
| 401 | L2 AIC 将 Wait(4) | 到达 Wait | 未到 Wait |
| 402 | L2 AIC Wait(4) 后 | **SET 配对成功** | 死等 SET |
| 500/510 | L2 AIV0/1 入口 | 该 AIV 已进 | 该 AIV 未进 |
| 501/511 | SET(4) 前 | 到达 SET 前 | 死在更早 |
| 502/512 | 已 SET(4) | 旗语已发 | 未发 SET |
| 900 | 哨兵 | 主动失败路径 | — |

---

## 7. 目标分解（图谱向上长）

```
积木可用 → 单段握手不挂(SIM) → 2-launch 骨架可重复跑完(SIM)
  → 逐步加真算仍不挂(SIM)
  → **卡死根因（优先；NPU_SUITE 数字 TRACE）**
  → **正确性（卡死点之后；ByteDecode/权威交叉暂缓）**
  → 实机无卡死且正确
```

当前：**k=2 形态粘合 + Â 完整 2×2 SampleNTT 已可 SIM 拼装**；握手不变量已采纳；**SIM 不能复现粘性挂**；**实机根因未钉死**。用户定调（2026-09-06）：**ByteDecode/正确性比对在卡死点之后，找出卡死原因前不做**。下一刀 = **卡死根因路径 → 筹备 NPU_SUITE 数字 TRACE**（禁复踩 retracted；禁正确性刀）。  
进行中：自底向上新开 `graph_tests/toys/toy-eNN-*`；每刀回写 §3/§4 + 刷图谱。

### 实验队列（短刀；禁复踩 §3）

| ID | 目录 | 要回答的新问题 | 不测什么 |
|----|------|----------------|----------|
| **E01–E03** | 握手 / SoftSync可选 / 阶段齐 | ✅ |
| **E04–E07** | 真 NTT→basemul→INTT（≠Tag5T 整图已标） | ✅ |
| **E08** | +真 CBD；采样+代数主链齐 | ✅ |
| **E09** | +真 Compress_d(d=4) | ✅ |
| **E10** | +真 ByteEncode_d(128B) | ✅ |
| **E11** | +真 Decompress_1(μ) | ✅ |
| **E12** | +k=2 多 poly 真几何 | ✅ |
| **E13** | Encrypt 形态粘合（两段角色 + c1∥c2） | ✅ |
| **E14** | +真 SampleNTT（Â 上排 2 poly；独立 launch） | ✅ |
| **E15** | +Â 完整 2×2 SampleNTT | ✅ |
| **E16** | `npu_suite/` 数字 TRACE 套件（C0/C1/C2；SIM smoke 绿） | ✅ |
| **下一步** | 用户整份上机填 `REPORT_TEMPLATE.md` → 主控按 `BRANCHING.md` 推理 | **等待实机数字** |
| **上机推理** | 测什么×反馈分支 → [`graph_tests/npu_suite/BRANCHING.md`](../../graph_tests/npu_suite/BRANCHING.md) | **已锁** |
| **缓** | t̂·ByteDecode / 权威交叉 / 更高 k | **卡死点之后再做** |

---

## 8. 指针（不搬码）

| 材料 | 用途 |
|------|------|
| `docs/rg-kem-encrypt-hang.yaml` | 旧排查图，失败节点可参考 |
| `ascendc-tests/fix-encrypt-skel-mix-chain-toy/` | 冻结只读：缺 SET⇒124 等 |
| `ascendc-tests/fix-encrypt-clean-hostmu-2launch/` | 冻结只读：P0/P1a SIM 绿 |
| `qa/2026-09/*` | 实机短报与纠偏 |
| `library/documents/CANN-AscendC算子开发接口参考-查阅索引.md` | CrossCore / SoftSync API |

---

## 变更

| 日期 | 内容 |
|------|------|
| 2026-09-06 | 初版：目标/分工/铁律/实机事实/失败优先/约束/积木/TRACE/目标分解 |
| 2026-09-06 | 加锁：短实验/禁复踩/每刀刷库图；层1握手采纳冻结证据；开 E01 队列 |
| 2026-09-06 | E01 PASS：8 轮 SIM 绿、OMIT⇒124；开 E02 SoftSync→SET4 |
| 2026-09-06 | E02 PASS：SoftSync→SET4 绿；OMIT SoftSync 仍绿→极简骨架 SoftSync 非必要；开 E03 |
| 2026-09-06 | E03 PASS：阶段齐 stub×3；层2收口；开 E04 真 NTT |
| 2026-09-06 | E04 PASS：骨架+真NTT 对拍；开 E05 L1 真 SHAKE |
| 2026-09-06 | E05 PASS：真 SHAKE+NTT；开 E06 basemul |
| 2026-09-06 | E06 PASS：真 basemul；开 E07 INTT |
| 2026-09-06 | 注：E06 basemul 为自包含标量 Alg.11/12 拷贝（非改原 multiplyntts 探针） |
| 2026-09-06 | E07 PASS：真 INTT；开 E08 CBD |
| 2026-09-06 | E08 PASS：主积木链齐；开 E09 Compress |
| 2026-09-06 | E09 PASS：真 Compress；开 E10 ByteEncode_d |
| 2026-09-06 | E10 PASS：真 ByteEncode；开 E11 Decompress(μ) |
| 2026-09-06 | E11 PASS：真 Decompress(μ)；开 E12 多 poly k=2 |
| 2026-09-06 | E12 PASS：k=2 多 poly；开 E13 Encrypt 形态粘合 |
| 2026-09-06 | E13 PASS：Encrypt 形态粘合；开 E14 SampleNTT(Â) |
| 2026-09-06 | E14 PASS：SampleNTT u×2（独立 launch）；开 E15 补齐 Â 2×2 |
| 2026-09-06 | E15 PASS：Â 完整 2×2；SIM 墙钟触顶，停派下一刀待策略确认 |
| 2026-09-06 | 用户定调：卡死优先；ByteDecode/正确性比对暂缓；开 E16 NPU_SUITE 数字 TRACE |
| 2026-09-06 | E16 PASS：npu_suite 包装+SIM smoke；等待实机 TRACE 回报 |
