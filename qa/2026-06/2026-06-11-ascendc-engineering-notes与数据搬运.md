# 2026-06-11 — engineering-notes、DataCopy 知识库、exp-int8 tiling、NTT 路线冻结

**记录时间**：2026-06-11 10:56（本地）

---

## 结论（拍板）

1. **`ascendc-engineering-notes` 定位**：指导 **AscendC 平台开发**（标量 / 向量 / 矩阵 / 数据搬运 / 组件同步），**不写**具体算子场景（NTT、模运算、merged_kyber 路线细节）。技术点讲清楚后适用于任意算子；若把「当前 NTT 路线」写进强制 Skill，会干扰他人用另一条路线实现同一算子。
2. **场景与路线纪要迁出**：NTT、limb6、`[2k,256]` vs `[16,512]` RouteA、merged_kyber vs Matmul 融合对照等 → `.cursor/skills/ascendc-engineering-notes/references/route-and-scenario-notes.md`（**非强制**；仅 customspec 或任务点名时读）。
3. **Matmul 融合 MIX / NTT 内 `Matmul<>`**（**6/11 晚间修订**）：当日早先「暂缓非禁止」已被下文 **[§ NTT-Matmul 路线废弃冻结](#ntt-matmul路线废弃冻结)** 取代 — **NTT 场景下 `Matmul<>` 路线废弃中止**。F203 批 NTT 固定 `merged_kyber` FSM + `AicMmad`。平台 Skill 中「纯 AIC 可用 `Matmul<>`」仅指与 NTT 无关的通用探针，且已归档者勿 fork。
4. **CPU vs SIM/NPU 同步（重申并写入 SKILL §7）**：
   - **CPU**：近似自上而下顺序执行，**不存在**真实的 MTE/Vector/Cube 并行，组件同步问题常被掩盖。
   - **SIM/NPU**：搬运与计算并行，**不保证**源码书写顺序；未同步易出现写未读完、读脏数据、误读误写。
   - **正确性阶段**：宁可多加 `PipeBarrier<PIPE_ALL>()` 与 MIX 的 `CrossCoreSetFlag`/`WaitFlag`；**SIM 通过后再**做 barrier 精简。**禁止**仅凭 CPU 通过就删除 SIM 必需同步。
5. **Stage3 / MatMul 调不通的主因（讨论共识）**：多半不是 NTT 公式错，而是 **矩阵布局（ND/NZ/ZZ/ZN）、Nd2Nz stride、GM 张量契约** 与 **跨 PIPE/跨核同步** 未掌握；须先读离线 CANN 教材与工程笔记再写 MIX / Gather 路径。

---

## 当日产出（文档 / Skill）

| 路径 | 变更 |
|------|------|
| `.cursor/skills/ascendc-engineering-notes/SKILL.md` | 重写为平台通则：SU/VU/MU、排布、DataCopy、MIX、**§7 CPU/SIM 同步**、离线 CANN 索引 |
| `.cursor/skills/ascendc-engineering-notes/references/route-and-scenario-notes.md` | **新建**；承接 NTT/merged_kyber/RouteA 等场景纪要 |
| `.cursor/skills/INDEX.md` | 区分强制 SKILL 与可选 `references/` |
| `.cursor/skills/pre-research/SKILL.md` | 写码前强制读 SKILL；场景纪要非强制 |
| `.cursor/skills/ascendc-delivery/SKILL.md` | 同上 |

---

## 讨论要点

### 离线 CANN 教材（`thirdparty/ntt_study/html/`）

用户提供的四份离线页，用于补齐 Agent 在 **AI Core 数据搬运** 上的知识缺口：

| 文件 | 用途 |
|------|------|
| `AscendC 硬件架构 基本架构-CANN商用版8.0.0-昇腾社区.html` | AIC/AIV 分离、MTE、GM 典型数据流 |
| `AscendC矩阵基础知识-CANN商用版8.0.RC3-昇腾社区.html` | Matmul 数据流、baseM/N/K、多核/核内切分 |
| `AscendC矩阵数据排布格式-CANN商用版8.5.0-昇腾社区.html` | ND/NZ/ZZ/ZN、分形、ND→NZ |
| `随路格式转换-DataCopy-…HarmonyOS开发者.html` | `Nd2NzParams` 各字段、GM→A1 通路 |

辅以 `thirdparty/ntt_study/docs/engineering/新场景设备架构和使用方式概况.txt`（block/repeat/stride、SET/WAIT_FLAG）。

**阅读顺序**（已写入 SKILL §10）：排布格式 → 矩阵基础 → DataCopy → 硬件架构 + txt。

### 为何 Stage3 / 旧 MatMul 路线容易错

| 层面 | 典型错误 | 平台层应对 |
|------|----------|------------|
| GM 契约 | 逻辑 `[16,512]` RouteA 与紧凑 `[2k,256]` limb6 混用 | customspec 写清 shape + stride；kernel 与 `gen_data` 同一契约 |
| Cube | 高阶 `Matmul<>` 隐藏 ND2NZ；tiling 与中间维错位 | MIX 内用手写 `AicMmad` + 显式 `Nd2NzParams`（本仓当前路线，见 references） |
| 向量 | 32B block、repeatStride 与 poly 间隔不一致 | §5 搬运六项自检 |
| 同步 | CPU 过、SIM 脏数据 | §7：先多 barrier，SIM 后再优化 |

### Skill 分层原则（6/11 定稿）

```text
强制加载：ascendc-engineering-notes/SKILL.md     → 算子无关平台通则
按需加载：references/route-and-scenario-notes.md → 本仓某条 NTT/MIX 路线
任务绑定：*-customspec.*                         → 当前算子张量契约与阶段划分
深度历史：docs/notes/、qa/                     → 定稿原理、对拍证据
```

---

## 承接 6/10 的代码状态（未在本日改码，供上下文）

| 用例 | CPU | SIM | 备注 |
|------|-----|-----|------|
| `fix-merged-kyber-ntt256-limb6-poly8-s123` | ✓ | ✓ | k=8 同 poly 探针 |
| `exp-sepolyvec8-ntt-k8` | ✓ | ✓ | k=8 互异随机；`seed=20260610` |
| `frozen-exp-mlkem-f203-stage12-encode-matmul-mix` | — | — | **废弃冻结** → `examples/frozen/` |
| `frozen-fix-merged-kyber-…-s12-matmul` | ✓ | ✗ | **废弃冻结** → `ascendc-tests/frozen/` |
| NPU 实机 | — | — | **待补** |

验收示例：

```bash
cd examples/incubating/exp-sepolyvec8-ntt-k8
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

---

## SIM/CPU 耗时启发式（笔记，非 Skill / Rule）

**记录时间**：2026-06-11（Tag5T `fix-f203-tag5t-ntt256-limb6-poly8-limbsplit-s123` 调通后）

**性质**：本仓探针跑出来的**经验笔记**；供人读、供 Agent 在卡顿时对照排查。**不**写入 `.cursor/rules/`、**不**升格为强制 Skill 条文。

### 观察

同类 **matmul + split** 探针（如 `fix-merged-kyber-…-block-s123`、`frozen-int8-matmul-cube-16x256x512`）在 CPU / `SIM_DIRECT` 下通常在 **~15s 内**跑完（含编译与 camodel 开销）。若整用例 **稳定超过 ~15s**，多半不是「SIM 本来就慢」，而是实现路径有问题。

### 案例：`InterleaveMatC` 标量 GM 拼列

| 版本 | SIM `Model RUN TIME` | 说明 |
|------|----------------------|------|
| Stage2 后接 `InterleaveMatC`（AIC 上双层 `for` 逐元素读写 GM） | **~64s** | 逻辑可对拍，但 AIC 标量 GM 与 Cube 算力不匹配 |
| 去掉拼列：`lut_stacked [512,256]` + 2× `AicMmad(16,256,256)` 直写 `mat_c [32,256]` | **~6.5s** | 与 block-s123 同量级 |

用 `KERNEL_COMPUTE_BUDGET_SEC=90` 只能证明「没死锁、数值对」，**不能**证明性能可接受；应先修路径，而不是先放宽 timeout。

### 超时优先排查（本仓常见）

1. **标量 GM 循环**（拼列、gather、逐系数 reduce）是否在 AIC/AIV 热路径上。  
2. **多余 workspace 搬运**（本可一次 Mmad / 一次 DataCopy 完成的中间态）。  
3. **错误 pass 同步**（CrossCore 死等、多余 `PipeBarrier` 叠在慢路径上——后者少见，标量 GM 更常见）。  

基线对照：修前 ~64s → 修后 ~6.5s（`ascendc-tests/frozen/frozen-fix-f203-tag5t-ntt256-limb6-poly8-limbsplit-s123`）。

---

## NTT-Matmul路线废弃冻结

**记录时间**：2026-06-11（晚间拍板）

### 结论（拍板）

1. **NTT 全链路内 Stage2 使用高阶 `Matmul<>` 的路线正式废弃、冻结**，状态：**路线废弃中止**（非「暂缓」）。
2. **本仓 F203 / Kyber 批 NTT 唯一主路径**：`thirdparty/merged_kyber` 手写 FSM — `AivSplit` → **`AicMmad`**×2 → `AivMerge`（见 `fix-merged-kyber-ntt256-limb6-poly8-s123`、`exp-sepolyvec8-ntt-k8`）。
3. 相关用例已迁至 **`ascendc-tests/frozen/`**、**`examples/frozen/`**，目录前缀 **`frozen-`**；代码与末次日志不再改动。
4. 平台 Skill 中「纯 AIC 可用 `Matmul<>`」仍指**与 NTT 无关的通用探针**，勿再接到 Kyber NTT Stage2。

### 冻结清单

各用例 **具体冻结/废弃原因** 见 [`ascendc-tests/frozen/INDEX.md`](../../ascendc-tests/frozen/INDEX.md)、[`examples/frozen/INDEX.md`](../../examples/frozen/INDEX.md)。

| 目录 | CPU | SIM | 说明 |
|------|-----|-----|------|
| `frozen-fix-merged-kyber-…-s12-matmul` | ✓ 两段式 | ✗ | Stage2 `pem_lsu invalid ldst addr` |
| `frozen-int8-matmul-cube-16x256x512` | ✓ | ✓（孤立） | 同 kernel 在 s12 Host 下 SIM 仍挂 |
| `frozen-int8-matmul-cube-128x512x512` | ✓ | ✗ | 多核 tiling 扫参探针 |

`examples/frozen/`：`frozen-exp-mlkem-f203-stage12-encode-matmul-mix`、`frozen-exp-mlkem-f203-stage2-int8-matmul-cube`。

### 经验教训（须保留）

1. **CPU 通过 ≠ 可交付** — SIM 是 GM 契约与同步的试金石。
2. **孤立 kernel 通过 ≠ 接入 MIX/多段 Host 通过** — Host buffer 布局与 launch 参数。
3. **勿在 NTT MIX 内用 `Matmul<>` 替代 `AicMmad`** — 中间态、`Nd2Nz`、CrossCore 不透明 vs `MachineState` 可审计。
4. **平台层共性** — ND/NZ/stride 与 PIPE/CrossCore；正确性阶段宁可多加 barrier。

### 仍在维护（对照）

| 用例 | Stage2 | CPU | SIM |
|------|--------|-----|-----|
| `fix-merged-kyber-ntt256-limb6-poly8-s123` | `AicMmad` | ✓ | ✓ |
| `fix-merged-kyber-ntt256-limb6-poly8-block-s123` | `AicMmad` | ✓ | ✓ |
| `exp-sepolyvec8-ntt-k8` | `AicMmad` | ✓ | ✓ |

---

## DataCopy 知识库归档

### 结论

1. **文档归位**：原 `ascendc-tests/DATACOPY_SLICE_LESSONS.md` **已撤销**；定稿知识库迁至  
   **`docs/notes/ascendc-DataCopy与数据搬运知识库.md`**（本仓 DataCopy/MTE/GM 搬运 **唯一长文知识库**）。
2. **内容范围**：合并 fix-f203 **SliceInfo 切片实验**；`thirdparty/ntt_study/docs/engineering/新场景设备架构和使用方式概况.txt` 中 **block/repeat/stride、MTE 通路、同步**；本文 **CANN 教材索引、六项自检、InterleaveMatC 耗时、CPU/SIM 同步**；Skill §5 DataCopy / Nd2Nz 要点。
3. **分层不变**：强制 Skill 仍为 `ascendc-engineering-notes/SKILL.md`（通则）；本知识库为 **按需深读**；场景路线仍见 `references/route-and-scenario-notes.md`。

### 触发原因

- 用户要求：**不在 ascendc-tests 下放文档**；归档到 `docs/` + `qa/` 讨论材料。  
- fix-f203 Stage3 尝试 `DataCopy` 切片取数失败后回滚 Gather，需沉淀可复用经验。

### 关键拍板（知识库摘要）

| 主题 | 要点 |
|------|------|
| API 谱系 | `DataCopy(count)` / `DataCopyParams`(32B) / `SliceInfo`(07_0105) / `DataCopyPad`(字节) **勿混** |
| 32B | block 最小粒度；搬运长度与 Vector stride 算错 → **踩踏** |
| SliceInfo 失败 | 8B 偶列间距 + 紧凑 UB + blockCount 整除 + dst 32B 对齐 |
| 生产路径 | 整行 `DataCopy` + `Gather` 字节索引；或改 GM 为平面布局 |
| 同步 | CPU 掩盖；SIM 必查 PipeBarrier / CrossCore |
| 性能 | 标量 GM 热路径、多余 workspace 搬运 → SIM 数十秒级劣化 |

### 产出路径

| 路径 | 说明 |
|------|------|
| [docs/notes/ascendc-DataCopy与数据搬运知识库.md](../../docs/notes/ascendc-DataCopy与数据搬运知识库.md) | **定稿知识库** |
| ~~ascendc-tests/DATACOPY_SLICE_LESSONS.md~~ | 已删除 |
| `.cursor/skills/ascendc-engineering-notes/SKILL.md` §5 / §10 | 增加指向知识库的链接 |

探针：`ascendc-tests/frozen/frozen-fix-f203-tag5t-ntt256-limb6-poly8-limbsplit-s123/`

---

## exp-int8-matmul 多核 tiling 实验

**用例**：[`ascendc-tests/int8-matmul-cube-128x512x512/`](../../ascendc-tests/int8-matmul-cube-128x512x512/)（2026-05-19 自 `examples/incubating/` 迁入；**实验与定稿在 6/11**）  
**平台**：CANN 9.0.0 · Atlas A2（910B4）· KernelLaunch CPU 孪生  
**验收**：`scripts/verify_result.py`，`max_abs_diff=0`  
**规范指南**：[融合算子多核 tiling 策略指南.pdf](../../docs/specs/ascendc/融合算子多核tiling策略指南.pdf)

### 实验目标

在纯 Cube **int8×int8→int32** MatMul（$C[128,512]=A[128,512]\times B[512,512]$，ND）上，验证 **多 AI Core 单次 launch** 的 tiling 闭合条件；内核骨架对齐 `MatmulInvocationNeo`（`CalcGMOffset` + `SetTail` + `IterateAll`）。

### 两层分块（核心认知）

| 层级 | Host API / 字段 | 决定什么 |
|------|-----------------|----------|
| **宏分片** | `SetDim` / `SetSingleShape` / `GetTiling` → `singleCoreM/N/K` | 哪个 `blockIdx` 算 $C$ 的哪一块 |
| **微分片** | `SetFixSplit(baseM, baseN, baseK)` | 每个 AIC **内部** L0/Cube 怎么 tile |

**Launch（910B）**：`blockDim` = AIC 数；`SetDim(usedCoreNum)` 取 **`usedCoreNum = blockDim × 2`**。无 `SetSingleShape` 时宏分块由 `GetTiling` 自动决定，**不一定**等于 `blockDim`。

### 实验记录（摘要）

| 配置 | 结果 | 要点 |
|------|------|------|
| 1 AIC，`SetFixSplit(16,32,-1)` | ✓ | 基线 |
| 4 AIC，仅改 launch + `SetDim` | ✓ | `GetTiling` 自动约 4 路 N 切分 |
| 8 AIC，`baseM=baseN=32` | ✗ | SIGFPE / KFC；微块与 int8 最小 tile 冲突 |
| 8 AIC，`baseM=16` | ✓ | **关键**：`baseM` 须为 16 倍数 |
| 16 AIC，无 `SetSingleShape` | ✗ | `GetTiling` 仍 8 路，`blockIdx≥8` 越界 |
| 16 AIC + 错误 `SetSingleShape` | ✗ | 网格积 ≠ `usedCoreNum` → `tailN` 负 |
| 16 AIC + 闭合 `SetSingleShape` | ✓ | 如 `(32,64,K)`、`(64,32,K)`、`(16,128,K)` 网格积均为 32 |

### tiling 闭合条件（定稿）

在 **`baseK=-1`**、**`SingleCoreK=K`** 前提下：

1. 最小 tile **$16\times32\times16$** → `baseM` 为 **16 倍数**，`baseN` 为 **32 倍数**。  
2. `SingleCoreM % baseM == 0`，`SingleCoreN % baseN == 0`。  
3. $\dfrac{M}{\text{SingleCoreM}}\times\dfrac{N}{\text{SingleCoreN}}=\text{usedCoreNum}$（整除）。  
4. **`blockDim = usedCoreNum / 2`**（910B）。

**配参顺序**：定 `usedCoreNum` → 因子分解网格 → `SetSingleShape` + `SetDim` + `SetFixSplit`。

### 产出与遗留

| 产出 | 路径 |
|------|------|
| 研究笔记 | [docs/notes/AscendC-多核MatMul-tiling技术总结.md](../../docs/notes/AscendC-多核MatMul-tiling技术总结.md) |
| 规范 | [融合算子多核tiling策略指南.tex](../../docs/specs/ascendc/融合算子多核tiling策略指南.tex) |
| 冻结探针 | `ascendc-tests/frozen/frozen-int8-matmul-cube-128x512x512/`（SIM 未过，见 [§ NTT-Matmul 路线废弃冻结](#ntt-matmul路线废弃冻结)） |

- [ ] `GetTiling` 失败时 host `exit`；NPU 实机复验 8/16 AIC  
- [ ] 反哺 `exp-sepolyvec8-ntt-k8` Stage2 多核 MatMul

---

## 相关归档

- **DataCopy 知识库（6/11 定稿）**：[docs/notes/ascendc-DataCopy与数据搬运知识库.md](../../docs/notes/ascendc-DataCopy与数据搬运知识库.md)；讨论见本文 [§ DataCopy 知识库归档](#datacopy-知识库归档)
- 平台 Skill：`.cursor/skills/ascendc-engineering-notes/SKILL.md`
- 场景纪要：`.cursor/skills/ascendc-engineering-notes/references/route-and-scenario-notes.md`
- 6/10 MIX/limb6：[docs/notes/F203-merged-kyber-MIX路线技术总结.md](../../docs/notes/F203-merged-kyber-MIX路线技术总结.md)、[批 NTT 总结](../../docs/notes/merged-kyber-poly-batch-NTT技术总结.md)
- 6/10 讨论：[2026-06-10-F203-MIX-merged_kyber路线与limb6.md](2026-06-10-F203-MIX-merged_kyber路线与limb6.md)

---

## 后续

- [x] 原 `docs/research/20260610-*.md` 已迁入 `docs/notes/` 并重构（2026-06-18）
- [x] NTT 内 `Matmul<>` 路线废弃冻结 → 本文 [§ NTT-Matmul 路线废弃冻结](#ntt-matmul路线废弃冻结)
- [ ] `exp-sepolyvec8-ntt-k8` 补 NPU `run.sh -r npu`
- [ ] 融合 MIX 路径：在弄清 GM 中间态 + CrossCore 后单独开探针，不与 merged_kyber 路线混写进强制 Skill
- [ ] 考虑将 batch 版 `aiv_func.hpp` 回灌 `thirdparty/merged_kyber`（减少 exp/探针重复维护）
