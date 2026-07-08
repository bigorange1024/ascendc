---
name: ascendc-engineering-notes
description: >-
  AscendC 平台工程通则：标量/向量/矩阵、数据搬运与排布、PIPE 同步、MIX 跨核、CPU 与 SIM/NPU 差异、本仓 KernelLaunch 验收。
  pre-research 与 ascendc-delivery 写码前强制阅读。算子场景路线见 references/（非强制）。
---

# ascendc-engineering-notes — AscendC 平台工程（轻量）

**性质**：指导 **AscendC 内核开发** 的平台级约束（与具体算子无关）。写/改 `examples/`、`ascendc-tests/` 内内核前由 **pre-research**、**ascendc-delivery** 强制加载。

**不算子场景**：NTT、模运算、某条 merged_kyber 路线等 → [references/route-and-scenario-notes.md](references/route-and-scenario-notes.md)（**仅** customspec 或任务点名时读）。

**深度教材**：`thirdparty/ntt_study/`、`docs/notes/`、`qa/`。  
**DataCopy / 搬运知识库（定稿）**：[`docs/notes/ascendc-DataCopy与数据搬运知识库.md`](../../../docs/notes/ascendc-DataCopy与数据搬运知识库.md)（讨论 [`qa/2026-06/2026-06-11-…#DataCopy`](../../../qa/2026-06/2026-06-11-ascendc-engineering-notes与数据搬运.md#datacopy-知识库归档)）。  
**TQue / Pipe 知识库（定稿）**：[`docs/notes/ascendc-TQue与Pipe框架知识库.md`](../../../docs/notes/ascendc-TQue与Pipe框架知识库.md)。

---

## 1. 本仓交付范式（工程壳，非算子逻辑）

| 采用 | 不采用（除非用户明确要求） |
|------|---------------------------|
| **KernelLaunch**：`*_custom.cpp` + `main.cpp` + `run.sh`（cpu / sim / npu） | ascend-kernel（op_host → whl → torch_npu） |
| `gen_data.py` + `verify_result.py`，数值对拍 | 无 bin 对拍即 claim 通过 |
| `ascendc-tests` 先 CPU+SIM，再 NPU | 未 sim 通过即交付 |

| 路径 | 用途 |
|------|------|
| `examples/incubating/exp-*` | 预研 + `*-customspec.*` |
| `examples/stable/stable-*` | 定型交付 |
| `ascendc-tests/*` | 平台探针（无 customspec，不晋级 stable） |

平台：**Atlas A2**（AIC/AIV 分离）。见 Rule `ascendc-development.mdc`。

---

## 2. 硬件与三类计算（SU / VU / MU）

**分离架构（Atlas A2）**：矩阵 **AIC（Cube）** 与向量 **AIV（Vector）** 为独立核，经 **GM** 交换数据；各有 Scalar，MIX 下同 launch 多段代码。

| 单元 | 别名 | 典型流水 |
|------|------|----------|
| 标量 SU | Scalar | 控制、地址、标量运算；**型号相关**是否可直接访 GM |
| 向量 VU | AIV, Vector | `GM → UB → [Vector] → UB → GM`（MTE2/MTE3） |
| 矩阵 MU | AIC, Cube | `GM → L1 → L0A/L0B → [Cube] → L0C → FixPipe → GM`（MTE1/2） |

**搬运单元 MTE** 与计算单元**并行**；不插入同步时，读写顺序在 SIM/NPU 上**不保证**与源码书写顺序一致（见 §7）。

更细：`thirdparty/ntt_study/docs/engineering/新场景设备架构和使用方式概况.txt`（block/repeat/stride、SET_FLAG/WAIT_FLAG）。

---

## 3. 向量（VU）要点

- 最小粒度 **32B**（一个 **block**）；不足仍占 32B，stride 算错会**踩踏**邻接数据。
- 一次常处理最多 **8 blocks = 256B**（一个 **repeat**）；`blockStride`、`repeatStride`、`repeatTimes` 须与 GM 布局一致。
- 数据在 **UB**（VECIN / VECCALC / VECOUT）；与 GM 间用 `DataCopy`。

---

## 4. 矩阵（MU）与数据排布

### 4.1 逻辑与格式

- 矩阵乘 \(C = A B\)（+ bias）；\(A[M,K]\)、\(B[K,N]\)、\(C[M,N]\)。
- GM 上多为 **ND**；进 L0：**A→ZZ，B→ZN，C→NZ**（分形 fractal，与 dtype 相关）。
- **ND→NZ**：pad → reshape → transpose；随路转换用 `DataCopy(..., Nd2NzParams)`。

**int8 常见分形（half 系文档可类推）**：ZZ 16×32、ZN 32×16、L0C NZ 16×16。详见 §10 离线 HTML。

### 4.2 存储层次与 tiling（高阶 Matmul / 手写 Mmad 均适用）

| 逻辑位置 | 角色 |
|----------|------|
| A1 / B1 | 大块缓存（≈ L1） |
| A2 / B2 | 核内切块（≈ L0） |
| CO1 / CO2 | 小块 / 整块结果 |

核内切分：**baseM × baseK** 与 **baseK × baseN** 累加得 **baseM × baseN**。`baseM/baseN/baseK` 须满足硬件最小粒度；int8 路径 K、N 常为 **32 的倍数**。

### 4.3 高阶 Matmul<> vs 低阶 Mmad / 手写搬运

| 方式 | 特点 |
|------|------|
| **`Matmul<>` 高阶 API** | 封装 ND2NZ、Iterate、tiling；适合 **与 NTT 无关的纯 AIC 隔离**（**勿**用于 Kyber NTT Stage2 — 已废弃，见 [qa/2026-06/…#NTT-Matmul](../../../qa/2026-06/2026-06-11-ascendc-engineering-notes与数据搬运.md#ntt-matmul路线废弃冻结)） |
| **手写 `Mmad` + `DataCopy` + `Nd2NzParams`** | 显式 GM/L1/L0 布局；MIX 内需看清阶段间 GM 契约时更可控 |
| **融合 MIX 模板**（如 Matmul+LeakyRelu） | GM 中间态与 CrossCore、Iterate **耦合**；未弄清前**暂缓**作全链路默认；弄清后仍可能适用 |

**禁止假设**：数学上的 shape 等于 GM 上实际 stride；高阶 API 输出布局必须与 customspec **一致**。

### 4.4 张量契约（算子无关）

customspec / `gen_data` / kernel 必须统一：

- `shape`、`dtype`、**行 stride / 块 stride**、ND 或 NZ、是否 padding  
- 多阶段 kernel：**每阶段交接的 GM 张量**单独定义；不得用「另一条已验证路线」的布局默替换当前 spec  

抽象错位示例：逻辑 `[M,N]` 行优先 ND，若 `srcDValue` 按错误行宽配置，NZ 分形内容全错——与具体是 NTT 还是卷积无关。

---

## 5. 数据搬运（DataCopy）

**深度附录**：API 谱系（`DataCopyParams` / `SliceInfo` / `DataCopyPad`）、fix-f203 切片实验、MTE 通路 → [`docs/notes/ascendc-DataCopy与数据搬运知识库.md`](../../../docs/notes/ascendc-DataCopy与数据搬运知识库.md)。

### 5.1 普通搬运

`DataCopy(dst, src, len)` 等：注意 **32B 对齐**、源/目的位置（GM / UB / A1）。**切片取数**优先 `DataCopy + SliceInfo`（CANN 07_0105），勿默认 `DataCopyPad`。

### 5.2 随路 ND2NZ（`Nd2NzParams`）

写 kernel 前核对：

| 字段 | 含义 |
|------|------|
| `nValue` / `dValue` | ND 矩阵行数 / 列数 |
| `srcDValue` | 源 ND **相邻两行**起始间隔（元素） |
| `srcNdMatrixStride` | 多个 ND 矩阵块之间间隔 |
| `dstNzC0Stride` | ND 一行变 NZ 多行后，NZ 内相邻行间隔（**32B 单位**） |
| `dstNzNStride` | NZ 分形内 Z 型行间隔 |
| `dstNzMatrixStride` | 相邻 NZ 矩阵块间隔 |

通路示例：GM→A1/B1（矩阵侧）；VECIN/OUT→TSCM（向量侧，型号相关）。见 §10 DataCopy 离线页。

### 5.3 搬运自检（六项）

1. 逻辑 M/K/N 与 GM 实际布局一致？  
2. 本段 AIC 还是 AIV？通路是否 GM→L1→L0 或 GM→UB？  
3. `Nd2NzParams` 是否与 GM stride 一致？  
4. `baseM/N/K`、对齐约束是否满足？  
5. 向量长度、blockStride/repeatStride 是否 32B 对齐？  
6. 多阶段：上阶段写 GM 后，下阶段读之前是否已同步（§7）？

---

## 6. MIX 与跨核同步

- **MIX**：AIC + AIV 同 kernel launch；阶段间默认 **GM 可见性** + `CrossCoreSetFlag` / `CrossCoreWaitFlag`（或项目封装的等价物）。
- **单核内多 PIPE**：MTE / Vector / Cube **并行**；用 `PipeBarrier<PIPE_*>`、`SetFlag`/`WaitFlag`（或 `TPipe` 事件）保证先后关系。
- **`TPipe` / event**：每个算子实例 **Init 一次**；禁止在 batch 循环内反复构造会 `AllocEventID` 的 pipe 包装类。同 `TPosition` 上 **TQue 槽位有限**（910B AIV 常为 8）— 详见 [TQue 知识库](../../../docs/notes/ascendc-TQue与Pipe框架知识库.md)。
- **CPU 双 launch** 绕过 CrossCore：仅 **调试** CrossCore 逻辑，非 SIM/NPU 交付形态。

---

## 7. CPU 与 SIM/NPU：同步策略（重要）

| 模式 | 执行模型 | 同步含义 |
|------|----------|----------|
| **CPU** | 近似 **自上而下顺序执行** | **不存在**真实的 MTE/Vector/Cube 并行；组件间同步问题**常被掩盖** |
| **SIM / NPU** | 搬运与计算 **并行**，乱序完成 | 未同步 → **写未读完、读脏数据、误读误写**；CPU 过 ≠ 实机过 |

**首次实现（正确性阶段）**：

- **宁可多加**手动同步，不要赌顺序。  
- 单核：在 CopyIn → 计算 → CopyOut 之间、以及多 PIPE 交界，使用 `PipeBarrier<PIPE_ALL>()`（或按手册对 `PIPE_MTE2`/`PIPE_MTE3`/`PIPE_V`/`PIPE_M` 细分）。  
- MIX：AIC 写 GM 后 `CrossCoreSetFlag`，AIV 侧 `CrossCoreWaitFlag` 后再 `DataCopy`；反之亦然。  
- MTE 与 Vector/Cube 之间：遵循手册 **SetFlag/WaitFlag** 或 `TPipe::Enqueue` 事件模型。

**性能 / 重构阶段**：

- 在 **SIM/NPU 仍通过** 的前提下，再分析哪些 barrier 冗余并删减。  
- **禁止**：仅凭 CPU 通过就删除 SIM 上必需的 barrier。

本仓 merged_kyber 系探针中常见 `PipeBarrier<PIPE_ALL>()` 较密，属正确性优先策略，见 `thirdparty/merged_kyber/mmad_custom.cpp` 等。

---

## 8. 验收命令

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
python3 scripts/verify_result.py
```

- **Agent 强制双模式**：声称通过前须 **CPU + SIM_DIRECT sim 都跑**；细则见 `.cursor/rules/ascendc-development.mdc`「Agent 跑用例验收」。  
- **SIM dump 路径**：kernel **前** `source scripts/camodel_sim_log.sh "${CURRENT_DIR}"`；kernel **后** `camodel_sim_collect_stray "${CURRENT_DIR}"`；产物**只允许**在 `OPPROF_<ts>_<pid>/dump/`，**禁止**落在用例根目录。  
- **防挂死**：各用例 `run.sh` 设 `KERNEL_COMPUTE_BUDGET_SEC` + `scripts/kernel-run-timeout.sh`（**非**全仓 15s；见 [内核计算超时与性能定标.md](../../docs/engineering/内核计算超时与性能定标.md)）。  
- **NTT 全流程 SIM ~15s**：仅 Tag5T NTT 集成类探针的性能定标，不适用于 KeyGen/Encrypt 全链等。

**验收顺序**：CPU 可快速查逻辑；**SIM 是同步与搬运问题的试金石**；NPU 最后。

---

## 9. 写码检查清单（平台级）

- [ ] customspec 是否定义 GM 张量契约（shape、stride、layout）？`gen_data` 与 kernel 是否一致？  
- [ ] 本段 SU/VU/MU？数据通路？多阶段 GM 交接是否定义并同步？  
- [ ] Cube：`Nd2Nz` / ZZ·ZN·NZ 是否正确？`baseM/N/K` 与对齐？  
- [ ] 向量：32B block、repeat/blockStride？  
- [ ] **SIM/NPU**：关键 CopyIn/计算/CopyOut、MIX 跨核处是否有 barrier / CrossCore？（勿仅依赖 CPU 顺序）  
- [ ] 验收是否 **CPU + SIM** 双跑？SIM 是否 `camodel_sim_log.sh` 且 dump 不在用例根？  
- [ ] 单 `TPipe`、无循环内重复 pipe 初始化？  
- [ ] 改的是 exp / stable / ascendc-tests？门禁与 `STATUS`/`qa` 是否更新？  
- [ ] 算子路线是否误用本 Skill？场景纪要见 [references/route-and-scenario-notes.md](references/route-and-scenario-notes.md)

---

## 10. 离线 CANN 文档（`thirdparty/ntt_study/html/`）

| 文件 | 主题 |
|------|------|
| `AscendC 硬件架构 基本架构-CANN商用版8.0.0-昇腾社区.html` | AIC/AIV、MTE、GM 数据流 |
| `AscendC矩阵基础知识-CANN商用版8.0.RC3-昇腾社区.html` | Matmul 数据流、baseM/N/K、切分 |
| `AscendC矩阵数据排布格式-CANN商用版8.5.0-昇腾社区.html` | ND/NZ/ZZ/ZN、分形 |
| `随路格式转换-DataCopy-…HarmonyOS开发者.html` | `Nd2NzParams`、`DataCopy` |

**阅读顺序**：排布格式 → 矩阵基础 → DataCopy → 硬件架构 + `新场景…概况.txt` + [DataCopy 知识库](../../../docs/notes/ascendc-DataCopy与数据搬运知识库.md)。

---

## 11. 外部 Skill（可选）

| 目录 | 用途 |
|------|------|
| `vendor/cann-operator-env-config` | CANN 环境 |
| `vendor/npu-smi` | 设备状态 |
| `vendor/ascend-profiling-anomaly` | NPU profiling |

勿用 `ascendc-operator-dev` 在本仓新建 ascend-kernel 工程。
