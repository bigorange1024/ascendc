# CANN Ascend C 算子开发接口参考 — 查阅索引

**对应 PDF**：[CANN社区版 9.0.0 Ascend C算子开发接口参考 01.pdf](CANN社区版%209.0.0%20Ascend%20C算子开发接口参考%2001.pdf)  
**文档版本**：01（2026-05-27）  
**用途**：PDF 体积大（约 27MB），本文件记录**查阅过的主题**在 PDF 中的**章节 / 页码**及**简要概括**，便于后续快速定位。每次在对话或开发中查阅该 PDF 的 API / 约束 / 架构相关内容后，应**追加一条**「查阅记录」。

**离线网页**（社区站归档，与 PDF 部分章节对应）见 [../offline-web/INDEX.md](../offline-web/INDEX.md)。

---

## 查阅工作流（强制）

写码或设计用到 AscendC API 时，**按序**执行：

1. **先查本索引**：在下方「查阅记录」与「常用子节」表中搜索 API 名；有记录则按 PDF 页码 / 在线链接与「概括」列实现。
2. **索引无记录**：查 [CANN社区版 9.0.0 Ascend C算子开发接口参考 01.pdf](CANN社区版%209.0.0%20Ascend%20C算子开发接口参考%2001.pdf)（可用 `pdftotext` + 目录页码，或社区在线 API 页）。
3. **查完必写回**：在「查阅记录」**顶部**追加一行（日期、主题、章/节/页、约束与用法概括）；避免后续重复翻 PDF。

**审查红旗**：实现里用了 `Compares`/`GatherMask`/`GetCmpMask` 等，但索引与 git 历史均无对应查阅记录。

---

## PDF 顶层目录（页码摘自 PDF 目录页）

| 章 | 标题 | 起始页 |
|----|------|--------|
| 1 | Ascend C API 列表 | 1 |
| 2 | **SIMD API**（本项目当前采用） | 53 |
| 3 | SIMT API（暂不采用） | 2345 |
| 4 | Utils API | 2954 |
| 5 | AI CPU API | 3147 |
| 6 | 附录 | 3153 |

### 第 2 章 SIMD API — 常用子节

| 节 | 标题 | 起始页 |
|----|------|--------|
| 2.1 | 通用说明和约束 | 53 |
| 2.2 | 基础数据结构 | 57 |
| 2.3 | 基础 API | 136 |
| 2.3.1 | Memory 数据搬运 | 136 |
| 2.3.2 | 矩阵计算（ISASI） | 216 |
| 2.3.3 | Memory 矢量计算 | 357 |
| 2.3.3.4 | **比较与选择**（Compare / Compares / Select / GatherMask 等） | 549 |

---

## 查阅记录

按时间倒序追加（最新在上）。

### 2026-06-23 — Alg.7 SampleNTT rej 向量 compact / 比较掩码链（pass-fix-f203-alg7-sample-ntt-k4）

| 查阅主题 | PDF / 在线位置 | 概括 |
|----------|----------------|------|
| **探针背景** | — | [`ascendc-tests/pass-fix-f203-alg7-sample-ntt-k4`](../../ascendc-tests/pass-fix-f203-alg7-sample-ntt-k4/)：rej 后 stream compact 取前 256 个 â；R5 草稿误用 `Compares(LT/NE)` + `GetValue(dst)!=0` 导致 SIM 失败，根因是**未按本索引/PDF 查 API**。 |
| **Compares** | 2.3.3.4.3，**p.558**；[07\_0068](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0068.html) | 逐元素与**标量**比较；`dst` 为 **`uint8_t` bit 打包**（小端，每 bit 对应 src0 一元素），**不是**每 lane 一字节 0/1，禁止 `GetValue(b)!=0` 判真。A2：`half/float` 支持全部 `CMPMODE`；**`int32_t` 仅 `CMPMODE::EQ`**（与 `Compare` 同，见 2026-05-19 条）。Level-1 `count` 接口：**count 个元素所占空间须 256B 对齐**（int32 常用 count=128）。`dst`/`src0` 起始地址 **32B 对齐**。 |
| **Compare（结果存入寄存器）** | 2.3.3.4.2，**p.554**；[07\_0067](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0067.html) | 双 tensor 比较，结果写入 **CmpMask 寄存器**（非 `dst` LocalTensor）；配合 **`GetCmpMask`** 读出。float/half 上 `LT` 等可用；int32 仅 EQ（A2）。compact 备选：**`Compare` + `GetCmpMask`** 得掩码再 `GatherMask`。 |
| **GetCmpMask(ISASI)** | 2.3.3.4.5，**p.568**；[07\_0223](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0223.html) | 读取 **`Compare`（寄存器版）** 后的 CmpMask；`dst` 为 `uint8_t` LocalTensor，**≥128B**，**16B 对齐**。示例：float `Compare(LT)` 后 `GetCmpMask` 得按 bit 展开的掩码字节。与 **`Compares` 的 `dst` LocalTensor** 是不同路径。 |
| **SetCmpMask(ISASI)** | 2.3.3.4.6，**p.570** | 为**不传 mask 的 `Select`** 预设比较寄存器；`SELMODE::VSEL_CMPMASK_SPR` 等。与 `GetCmpMask` 对称。 |
| **Select** | 2.3.3.4.7，**p.572**；[07\_0070](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0070.html) | 按 **`selMask` 比特**：1→取 `src0`，0→取 `src1`（或标量模式 1）。A2 支持模式 0/1/2。rej **`vec_mask` 路径**：`Compares(EQ)` 得 bit 掩码后，可用 **`Select`** 在 stream 与 0 间选路（需正确理解 `selMask` 布局）。高阶封装另见 [07\_0859](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0859.html)。 |
| **GatherMask** | 2.3.3.4.9，**p.588**；[07\_0071](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0071.html) | 按 **gather mask**（内置模式或用户 `src1Pattern` Tensor）从 `src0` **收集**元素写入 `dst`；`rsvdCnt` 输出保留元素个数。A2：`reduceMode=true` **Counter 配置方式一**（每 repeat `mask` 个元素）。`src1Pattern` 类型随 `T`：float/int32 用 `uint32_t` 等。compact 方向：**掩码 bit / `GetCmpMask` 结果 → `GatherMask` 压紧**，分 tile 累加至 256。 |
| **Mins** | 2.3.3.1.24，**p.437**；[07\_0057](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0057.html) | 逐元素与标量取 min：`dst[i]=min(src[i], scalar)`。Alg.7 rej **`vec_mins`（默认）**：`int16` stream 与 `q` 标量 `Mins` 实现 `<q` 剔除（等价 `Compares(LT)` 但无 bit 掩码）。 |
| **Compare / CompareScalar（对照）** | 见 2026-05-19 条；[07\_0066](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0066.html) | A2 **`int32` 仅 EQ**；`GE/LT` 需 float/half 链。`Compares` 与 `Compare` 约束一致，但输出形态不同（LocalTensor bit 包 vs 寄存器）。 |

**R5 compact 正确路线（待实现）**：`int16` 上 `Mins` 已用于剔除；compact 应用 **`Compare(EQ)` 或 float 链 `Compare(LT)` + `GetCmpMask`**，或 **`Compares`（仅 EQ on int32）** 生成掩码，再 **`GatherMask`** 分块前缀压紧；**禁止** int32 `Compares(LT/NE)` 与把 `Compares` 的 `uint8` dst 当逐 lane 布尔读。

### 2026-06-19 — Alg.13 行 16–20 2s1e UB 融合 customspec（FIPS CBD 采样）

| 查阅主题 | PDF / 在线位置 | 概括 |
|----------|----------------|------|
| **2s1e Alg13 customspec** | — | [`exp-mlkem-f203-alg13-16171820-2s1e-k4-实现方案-customspec.tex`](../../examples/incubating/exp-mlkem-f203-alg13-16171820-2s1e-k4/exp-mlkem-f203-alg13-16171820-2s1e-k4-实现方案-customspec.tex)：行 16–20；MIX `blockDim=1`；平面 mat\_c；UB 融合；**Host FIPS SamplePolyCBD s/e**（禁止 FIXED\_POLY）。 |
| **CrossCore / DataCopy / Add** | 见 Stage12、Stage1 查阅记录 | 与 vec-k4-v2 同构；NTT S1–S3 无 Gather。 |
| **Alg.11 Gather** | 2.3.3 | 仅行 18 basemul；非 NTT 平面读。 |

### 2026-05-19 — F203 Stage1+2 融合 customspec（MIX、aicore=1）

| 查阅主题 | PDF / 在线位置 | 概括 |
|----------|----------------|------|
| **Stage12 customspec** | — | [`exp-mlkem-f203-stage12-encode-matmul-mix-实现方案-customspec.tex`](../../examples/incubating/exp-mlkem-f203-stage12-encode-matmul-mix/exp-mlkem-f203-stage12-encode-matmul-mix-实现方案-customspec.tex)：Stage1 encode + Stage2 Matmul；`KERNEL_TYPE_MIX_AIC_1_2`；`blockDim=1`；`CrossCore*` S1→S2。 |
| **CrossCoreSetFlag / CrossCoreWaitFlag** | API 列表 $\approx$p.15–16 | S1 AIV→AIC、S2 AIC→AIV pack；参考 [`pass-fix-f203-stage123-ntt-intt-polyvec8-vec`](../../ascendc-tests/pass-fix-f203-stage123-ntt-intt-polyvec8-vec/mmad_custom.cpp)。 |
| **Matmul / REGIST_MATMUL_OBJ** | Matmul 教程；Stage2 隔离用例 | `int8×int8→int32`；`aicore=1` tiling 闭合。 |
| **Stage1 向量 API** | 见 Stage1 customspec 查阅记录 | encode 主路径 100\% 向量。 |

### 2026-05-19 — F203 Stage1 纯向量 / Stage3 向量预研（Cast、算术、搬运、比较）

| 查阅主题 | PDF / 在线位置 | 概括 |
|----------|----------------|------|
| **Stage1 customspec** | — | [`exp-mlkem-f203-stage1-encode-vec-实现方案-customspec.tex`](../../examples/incubating/exp-mlkem-f203-stage1-encode-vec/exp-mlkem-f203-stage1-encode-vec-实现方案-customspec.tex)：数学 → 分块 → 全量 API 表 → 逐步覆盖率（主路径 **100% 向量**）。 |
| **Stage3 customspec** | — | [`exp-mlkem-f203-stage3-routea-mod-vec-实现方案-customspec.tex`](../../examples/incubating/exp-mlkem-f203-stage3-routea-mod-vec/exp-mlkem-f203-stage3-routea-mod-vec-实现方案-customspec.tex)：RouteA+mod；**int64 合并与 mod 标量缺口**（Div 无 int32、int64 无矢量算术）。 |
| **Cast** | 2.3.3；[07\_0073](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0073.html) | A2 **表 6**：无 `int32→int8` / `int16→int8`。910B 实测链：`int32→int16→half→int8`（`CAST_NONE`）。dav\_c220 `CheckCastDatatype` 与表一致。 |
| **ShiftRight** | 2.3.3；[07\_0059](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0059.html) | `int32_t` 算术右移；A2 scalar∈[0,32]。Stage1：`hi=v>>6`。 |
| **Muls** | 2.3.3；[07\_0055](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0055.html) | A2：`half/float/int16_t/int32_t`。Stage1：`hi*64`；Stage3 预研：`×4096/×64`（int32 有溢出风险）。 |
| **Sub** | 2.3.3；[07\_0036](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0036.html) | A2：`half/int16_t/int32_t/float`。Stage1：`lo=v-hi*64`。 |
| **Add / Mul** | 2.3.3；[07\_0035](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0035.html) / [07\_0037](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0037.html) | A2 支持 `int32_t`。Stage3：`hl+lh` 等（受 int64 合并约束）。 |
| **Div** | 2.3.3；[07\_0038](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0038.html) | 文档面向 half/float；**dav\_c220 仅 half/float**（`kernel_operator_vec_binary_impl.h`）。Stage3.1 **不能**直接 int32 向量除法。 |
| **Compare / CompareScalar** | 2.3.3；[07\_0066](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0066.html) | A2：`int32_t` **仅 CMPMODE::EQ**；`GE/LT` 等需 float/half。mod 双校正比较需标量或 float 链。 |
| **And** | 2.3.3；约 07\_0062 | A2：**int16_t/uint16_t**，非 int32。Stage1 未采用 `v&63`。 |
| **DataCopy** | 2.3.1；[07\_0101](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0101.html) | GM↔UB 连续块；Stage1/3 主搬运。 |
| **DataCopyPad** | 2.3.1 ISASI；[07\_0265](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0265.html) | A2 支持 `int32`；跨步 gather **备选**（Stage3 未用）。 |
| **Select** | 高阶；[07\_0859](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0859.html) | 掩码选路；mod 向量备选需配合 Compare。 |
| **TPipe / InitBuffer / TQue / TBuf** | 2.3 资源管理；[07\_0108](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0108.html) / [07\_0110](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0110.html) / [07\_0137](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0137.html) | 双缓冲流水；`TBuf` 作 `VECCALC`。 |
| **GlobalTensor / LocalTensor** | 2.2；[07\_0007](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0007.html) / [07\_0006](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0006.html) | `SetGlobalBuffer`；`GetValue`/`SetValue` 用于 Stage3 标量 tile。 |
| **GetBlockIdx** | 2.3 系统变量；[07\_0185](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0185.html) | 8 核各绑 1 poly。 |
| **KERNEL\_TASK\_TYPE\_DEFAULT** | Utils / 编程指南 | `KERNEL_TYPE_AIV_ONLY` 纯向量 Stage1/3。 |

### 2026-06-28 — F203 Encrypt 侧 Compress / Decompress / ByteEncode_d 探针

| 查阅主题 | PDF / 在线位置 | 概括 |
|----------|----------------|------|
| **F203 Compress_d** | [`fix-f203-compress-d-vec-k4`](../../ascendc-tests/fix-f203-compress-d-vec-k4/) | d=4：`Muls`+`Adds`+`ShiftRight`+`mask_low_bits`（256-wide）；d=10 设备暂标量 u64 Barrett。 |
| **F203 Decompress_d** | [`fix-f203-decompress-d-vec-k4`](../../ascendc-tests/fix-f203-decompress-d-vec-k4/) | d=4/10：`Muls(Q)`+`Adds(bias)`+`ShiftRight(d)`（256-wide）；bias=8/512。 |
| **F203 ByteEncode_d** | [`fix-f203-byteencode-d-vec-k4`](../../ascendc-tests/fix-f203-byteencode-d-vec-k4/) | d=4/10：256-wide `mask_low_bits_i32`（`ShiftRight`+`Muls`+`Sub`）；跨字节 pack 用 `GetValue`/`SetValue`（910B 无 Scatter）。布局同 mlk `poly_compress_d4/10_c` 打包半部。 |
| **F203 ByteDecode_d** | [`fix-f203-alg6-bytedecode-d-vec-k4`](../../ascendc-tests/fix-f203-alg6-bytedecode-d-vec-k4/) | d=4：128-wide widen + `mask_low_bits` 取低 nibble + 标量取高 nibble；d=10：64 组 5B→4 coef 标量 unpack（mlk 逆布局）。 |
| **mask_low_bits 模板** | 见 Stage1、`byte_encode12_vec.hpp` | `v mod 2^bits`：`ShiftRight(v,bits)` → `Muls(·,2^bits)` → `Sub(v,v,·)`；A2 支持 int32。 |
| **GetValue / SetValue** | [07\_0006](https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/900/API/ascendcopapi/atlasascendc_api_07_0006.html) | ByteEncode_d pack 阶段：nibble / 10-bit 交织写 `uint8` 输出（32/64 组循环）。 |

### 2026-06-09 — 项目平台基线与文档入口（初始化）

| 查阅主题 | PDF 位置 | 概括 |
|----------|----------|------|
| **开发工具链版本** | —（项目约定，非 PDF 单节） | 基于 **CANN 社区版 9.0.0 / AscendC 9.0.0** 开发。 |
| **目标硬件：Atlas A2** | 2.1 表 2-1（TPosition 映射，约 p.54） | Atlas A2 训练/推理系列：C1→L1 Buffer，C2→BiasTable Buffer，CO2→GM 等；与 GM / UB / L0A/B/C 等存储单元对应。 |
| **NPU / AI Core 规模（项目默认）** | —（项目约定） | Atlas A2 推训服务器常见 **8×NPU**；**每 NPU 默认最多 20 个 AI Core**（可按实机调整）。 |
| **AI Core 组成** | 离线 [基本架构](../offline-web/基本架构-CANN社区版9.0.0-昇腾社区.html)；PDF 1 章 API 列表 + 2.3.2/2.3.3 | 每 AI Core：**1×Cube（矩阵/AIC）+ 2×Vector（向量/AIV）**。官方分离架构下 Cube 与 Vector **无直接数据通路**；融合算子通过 **CrossCore\***、**CubeResGroupHandle**、**Fixpipe** 等机制协作（见下条）。 |
| **Cube–Vector 融合（无显式 GM 往返）** | 1 章 API 列表：CrossCoreSetFlag / CrossCoreWaitFlag（约 p.15–16）；CubeResGroupHandle / GroupBarrier（约 p.16）；2.3.2.1.12 Fixpipe（目录 p.276 起） | 样例（如 leakyrelu 类融合算子）中 Cube 输出可经硬件/运行时路径分发给对应 Vector，代码中往往**不见**「Cube→GM→Vector」显式搬运；更像 Cube0 结果**一分为二**供给 Vector0、Vector1。实现 MIX 算子时需结合 **GetSubBlockNum / GetSubBlockIdx** 区分 AIC/AIV。 |
| **Matmul+LeakyRelu 融合样例（Gitee samples）** | —（代码参考，非 PDF 单节） | `samples/operator/ascendc/tutorials/MatmulLeakyReluCustomSample/KernelLaunch/MatmulLeakyReluInvocation/matmul_leakyrelu_custom.cpp`：`Matmul<…, TPosition::VECIN>` 输出接 Vector 阶段；**工程技术抽象**（剔除激活公式）见 [qa/2026-06-09 纪要 §六](../../qa/2026-06/2026-06-09-AscendC平台与CANN文档索引.md)。 |
| **SIMD vs SIMT** | 目录：第 2 章 vs 第 3 章 | **本项目暂只采用 SIMD**（第 2 章）；SIMT（第 3 章，自 p.2345）后续再考虑。 |
| **SIMD 通用说明和约束** | **2.1**，p.53 起 | 头文件路径（`kernel_operator.h`、`basic_api` / `highlevel_api`）、TPosition↔物理内存表、地址对齐（表 2-2）、地址重叠约束。离线副本：[SIMD API 通用说明和约束](../offline-web/SIMD%20API通用说明和约束-CANN社区版9.0.0-昇腾社区.html)。 |
| **基本架构（昇腾分离架构）** | 离线 [基本架构](../offline-web/基本架构-CANN社区版9.0.0-昇腾社区.html)；PDF 中相关：2.1 存储映射、1 章分离模式 API | 离线页含：关键概念、AI Core 工作模式、计算/存储/搬运单元、典型数据流与指令流。PDF 接口参考以 **API + 约束** 为主，架构叙述以离线页 + 2.1 映射表互补。 |

---

## 维护约定

1. **新增查阅**：遵循上文「查阅工作流」；在「查阅记录」**顶部**表格中增加一行，写明日期、主题、PDF 章/节/页、约束与用法概括（含 A2 数据类型 / CMPMODE 限制）。
2. **页码**：优先写 PDF 印刷页脚页码（与目录一致）；若只有 `pdftotext` 行号，注明「约 p.XX」并在后续核对后修正。
3. **交叉引用**：若有对应离线网页或 `samples/` / `examples/` 代码，在概括列或本段下方用相对链接注明。
4. **子目录 INDEX**：若 PDF 本身增删改版，同步更新 [INDEX.md](INDEX.md) 中该 PDF 一行说明。
