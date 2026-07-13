# AscendC DataCopy 与数据搬运 — 知识库

**读者**：写 KernelLaunch 内核前需理解 GM↔UB 搬运的开发者  
**性质**：`docs/notes` 定稿；平台 Skill 轻量，本文件为 **深度附录**  
**Skill**：`.cursor/skills/ascendc-engineering-notes/SKILL.md`  
**讨论**：[qa/2026-06/2026-06-11-…#DataCopy](../../qa/2026-06/2026-06-11-ascendc-engineering-notes与数据搬运.md#datacopy-知识库归档)

---

## 0. 本文怎么读

| 章节 | 内容 |
|------|------|
| §1 | 存储层次与 MTE 通路 |
| §2 | VU 粒度：block / repeat / stride |
| §3 | MU 排布与 ND2NZ |
| §4 | DataCopy API 谱系（勿混用） |
| §5 | 搬运自检与方法论 |
| §6 | 附录：本仓案例 |

**阅读顺序（教材）**：矩阵排布 → 矩阵基础 → DataCopy → 架构 + 本库 §1–§5。

---

## 1. 存储与 MTE

### 1.1 讨论约定

L2、DMA、GM、DDR 等在工程讨论中 **先视为同一档片外存储（GM）**。

### 1.2 内部缓冲

| 位置 | 角色 |
|------|------|
| **GM** | 核间交换、阶段交接 |
| **UB** | 向量主路径；与 GM 间 `DataCopy` |
| **L1 / L0A/B/C** | 矩阵侧；Mmad 切块 |

**典型 MIX**：AIC 写 GM → `CrossCoreWait` → AIV `DataCopy` 进 UB → Vector → 写 GM。

### 1.3 MTE 方向

| 单元 | 示意 |
|------|------|
| MTE2 | GM → L0/L1/**UB** |
| MTE3 | **UB → GM** |

### 1.4 同步不变量

- 单核：`PipeBarrier` 保证 MTE 与 Vector/Cube 先后  
- MIX：`CrossCoreSetFlag/WaitFlag` 在 **GM 写完后、对核读前**  
- **CPU 顺序执行掩盖缺失 barrier**；SIM/NPU 是试金石  

---

## 2. VU 粒度（与 Vector API 一致）

| 概念 | 定义 |
|------|------|
| **block** | **32B** 连续物理槽；不足 32B 仍占 32B |
| **repeat** | 最多 8 blocks = 256B |
| **blockStride / repeatStride** | 以 32B 为单位的间隔 |

**推论**：`DataCopy` 搬进 UB 后，Vector 的 stride 必须与 **UB 内实际排布** 一致，否则「搬对、算错」。

---

## 3. MU：ND2NZ

Cube 进 L0 前常需 **ND→NZ**；`DataCopy(..., Nd2NzParams)` 随路转换。

写字段前核对：`nValue/dValue/srcDValue/dstNzC0Stride/...` 与 GM **实际 stride** 一致。

**NTT Stage2**：手写 `AicMmad` + 显式 `Nd2NzParams`；NTT MIX 内 **不用** `Matmul<>`（见 [NTT-Matmul路线废弃说明.md](NTT-Matmul路线废弃说明.md)）。

---

## 4. DataCopy API 谱系

| API | stride 单位 | 典型用途 |
|-----|-------------|----------|
| `DataCopy(dst, gm, count)` | 元素数（×sizeof 须 32B 对齐） | 连续块 |
| `DataCopy(dst, gm, DataCopyParams)` | **32B datablock** | 2D 矩形 |
| `DataCopy(dst, gm, SliceInfo[], …)` | 元素 stride；dst **32B 对齐** | 切片 |
| `DataCopyPad(...)` | **blockLen 为字节** | 非对齐、padding |

**误区**：07_0105 切片 = `DataCopy`+`SliceInfo`，**不是**默认 `DataCopyPad`。  
`DataCopyParams` 与 `DataCopyPad` **同名结构、不同实现**。

---

## 5. 方法论

### 5.1 写码前六项自检

1. 逻辑 shape = GM stride = gen_data = kernel？  
2. 本段 AIC 还是 AIV？通路 GM→UB 还是 GM→L0？  
3. `Nd2NzParams` 与 stride 一致？  
4. baseM/N/K 对齐满足？  
5. Vector blockStride 与 UB 布局一致？  
6. 多阶段：读 GM 前 barrier / CrossCore？

### 5.2 解交织策略决策树

```
偶/奇列间距是否 32B 整数倍？
  否 → SliceInfo 常失败 → 整行进 UB + Gather，或改 GM 契约为平面布局
  是 → 可试 SliceInfo / 分块 DataCopy
```

**长期优**：Stage2 写 **平面 GM**，CopyIn 一次连续 `DataCopy`（2s1e 路线）。

### 5.3 Init vs 热路径

固定 LUT、Gather 索引、reorder 表：**Init 阶段 `DataCopy` 进 UB**，Compute 只读 UB — 热路径禁止 `SetValue` 建索引。

### 5.4 CPU vs SIM

| | CPU | SIM/NPU |
|---|-----|---------|
| MTE/Vector 并行 | 无 | 有 |
| 缺失 barrier | 可能仍「对」 | 脏读 |

### 5.5 性能启发式（仅 ML-KEM Tag5T NTT / matmul+S123 类探针）

同类 **NTT 全流程**探针 SIM kernel 计算 **~15s 内**常见。稳定 **>~15s** 优先查：标量 GM 循环、多余 workspace 往返、CrossCore 死等。  
**不**适用于 KeyGen、Encrypt 全链、Compress/ByteEncode 等（见 `docs/engineering/内核计算超时与性能定标.md`）。

---

## 6. 附录：本仓案例

### 6.1 GM 布局族

| 布局 | 语义 | 地位 |
|------|------|------|
| ND 紧凑 `[2k,256]` | merged_kyber | frozen |
| 竖堆 `mat_c[32,256]` | Tag5T poly-batch | frozen |
| **平面 `[96,128]`** | 2s1e slot×half | **活跃** |
| RouteA `[16,512]` | Gather 解交织 | 废弃 |

### 6.2 Stage3 切片实验（limbsplit，历史）

偶列 GM 间距 **8B** → `SliceInfo` blockCount 不整除 / dst 非 32B 对齐 → **回滚**；生产用整行 `DataCopy`+`Gather` 或改平面契约。

### 6.3 Alg.11 ROM Init DataCopy

探针 `pass-fix-f203-alg11-12-multiplyntts-k4`：`MEM_OPS=1` 时 Init `DataCopy` γ+索引+reorder → SIM ~9k tick vs SetValue ~16k。

### 6.4 再试切片检查单

1. 画 src/dst 字节图，手算 blockCount  
2. dst 基址 % 32 == 0  
3. 读 07_0105 + `DataCopySliceGm2UBImpl`  
4. 评估 **改 GM 契约** 是否优于硬拗切片  

### 6.5 离线教材

`thirdparty/ntt_onnx/html/` — 排布、Matmul、`Nd2NzParams`、架构。  
CANN：[07_0101](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0101.html)、[07_0105](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0105.html)。

---

*2026-06-18：重构为原理优先；案例降为 §6。*
