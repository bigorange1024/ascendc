# 2026-06-09 — AscendC 平台基线与 CANN 文档索引

---

## 一、开发工具与硬件基线（用户确认，记入项目）

| 项 | 约定 |
|----|------|
| 工具链 | **AscendC / CANN 社区版 9.0.0** |
| 目标设备 | **Atlas A2** 推训服务器（常见 **8 块 NPU**）；**不支持 Ascend 310P** 等其它型号 |
| AI Core 规模 | **每 NPU 默认最多 20 个 AI Core**（实机可不同，以设备为准） |
| 单 AI Core | **1×Cube**（矩阵）+ **2×Vector**（向量）+ **Scalar**（标量，核内调度与标量算术） |
| 开发范式 | **暂只采用 SIMD**；SIMT 后续再纳入 |

---

## 二、架构要点（分离架构 vs 融合实践）

**官方分离架构**：Cube 与 Vector 之间**没有直接数据通路**；矩阵与向量任务历史上分属不同流水线。

**AscendC 融合实践**（样例中的 leakyrelu 等 MIX 算子）：

- 可在同一算子内**混合使用 Cube 与 Vector**；
- 代码里通常**没有**显式「Cube → GM → Vector」搬运；
- 行为上更接近：Cube 侧结果**一分为二**，分别交给同一 AI Core 上的 **Vector0、Vector1**（例如 Cube0 → Vector0 & Vector1）。

写 NTT / 内积等算子时，需区分四类执行资源：

- **Scalar**：普通 C 写在 `__aicore__` kernel 中，做 tiling 消费、索引/偏移/循环（见 §六）；
- 纯 Vector（AIV）；
- 纯 Cube（AIC）；
- MIX 融合（CrossCore、CubeResGroupHandle、Fixpipe 等，见 PDF 查阅索引）。

---

## 三、资料落盘

| 类型 | 路径 |
|------|------|
| 完整 API 参考 PDF | `library/documents/CANN社区版 9.0.0 Ascend C算子开发接口参考 01.pdf` |
| **PDF 查阅索引**（后续查 API 时追加） | [library/documents/CANN-AscendC算子开发接口参考-查阅索引.md](../../library/documents/CANN-AscendC算子开发接口参考-查阅索引.md) |
| 离线：基本架构 | [library/offline-web/基本架构-CANN社区版9.0.0-昇腾社区.html](../../library/offline-web/基本架构-CANN社区版9.0.0-昇腾社区.html) |
| 离线：SIMD 通用说明 | [library/offline-web/SIMD API通用说明和约束-CANN社区版9.0.0-昇腾社区.html](../../library/offline-web/SIMD%20API通用说明和约束-CANN社区版9.0.0-昇腾社区.html) |

**约定**：以后查阅上述 PDF 的具体 API / 约束时，在 **查阅索引** 中追加「章节 + 页码 + 简要概括」，避免重复全文检索。

---

## 四、与当日开工（ML-KEM-1024 KeyGen）的关系

- NTT（Alg.13 16–17 行）优先落在 **AscendC SIMD / AI Core**（Scalar + Cube + Vector，**KernelLaunch 范式**，见 §六）；
- 并行度与 launch 需按 **8 NPU × 20 Core/NPU** 的默认规模做 tiling 假设，实机再校准。

**Host 边界（用户确认，与 MatmulLeakyRelu 样例一致）**：

| 归属 | 做什么 | 不做什么 |
|------|--------|----------|
| **`main.cpp` 应用壳** | Golden bin 读写、GM 灌数、`launch`、同步、收结果；`exp-*` 调测入口同角色 | **不算子算法**：不算 tiling、不分块、不做 NTT/内积/模约减 |
| **AI Core 算子本体** | `*_tiling.cpp` + `*_custom.cpp`：Scalar 调度与 tiling；Cube/Vector 计算 | — |
| **其他 Host 服务（若需要）** | SHA3、encode/decode、与 liboqs 对拍等 **独立于 kernel 的 Host C** | 不混入 kernel 文件；仍待 T8 拍板 |

内积、编解码若进 **kernel** → 写在 `*_custom.cpp`（Scalar 普通 C 或 Vector）；若留 **Host** → 仅应用壳或对拍服务，不是算子本体。

详见 [2026-06-08 纪要 §六](2026-06-08-Rule-Skill落地与FIPS203-204终极目标.md) 与 [qa/TODO.md](../TODO.md) T2 / T8。

---

## 五、Gitee 官方 samples 更新（2026-06-09）

原 `samples/` 为 2026-05 从 **GitHub** `Ascend/samples` 克隆的旧树（`fc2aea9`），缺少 AscendC **Matmul+LeakyRelu 融合**样例。

| 项 | 说明 |
|----|------|
| 新克隆 | `git clone https://gitee.com/ascend/samples.git` → `samples/`（`master` `6511a5f`） |
| 旧树备份 | `samples-github-legacy-20260519/`（可删以省 ~221MB） |
| 用户给出的参考 | `samples/operator/ascendc/tutorials/MatmulLeakyReluCustomSample/KernelLaunch/MatmulLeakyReluInvocation/matmul_leakyrelu_custom.cpp` |
| 同类路径 | `operator/ascendc/0_introduction/12_matmulleakyrelu_frameworklaunch/`、`13_matmulleakyrelu_kernellaunch/`、`operator_contrib/MatmulLeakyReluCustom/` |

`MatmulLeakyKernel` 使用 `Matmul<…, MatmulType<TPosition::VECIN, …>>` + `LeakyReluCompute()`，即 Cube 矩阵乘与 Vector 激活在同一 AscendC 内核中衔接。工程技术抽象见 **§六**。

---

## 六、AscendC 融合算子写法抽象（KernelLaunch 范本，剔除激活公式本身）

> **参考路径**：`…/MatmulLeakyReluCustomSample/**KernelLaunch**/MatmulLeakyReluInvocation/`（目录名即范式：**直接 launch AI Core kernel**，算子本体在 NPU 侧）  
> 以下归纳 **AI Core（Scalar / Cube / Vector / 搬运）** 与 **应用壳（main）** 的分工，**不绑定** LeakyRelu 公式。  
> **硬件对照**（离线「基本架构」）：AI Core 含 **Cube、Vector、Scalar**；Scalar 负责核内指令发射与标量运算（含 tiling 与数据调度）。

### 6.0 KernelLaunch vs FrameworkLaunch（先分清范式）

| 范式 | 目录特征 | 算子计算落点 |
|------|----------|--------------|
| **KernelLaunch** | `KernelLaunch/…/MatmulLeakyReluInvocation` | **算子本体均在 AI Core**：Scalar（tiling + 调度）+ Cube + Vector；**Host 基本不做算法计算** |
| **FrameworkLaunch** | `FrameworkLaunch/…/op_host` + `op_kernel` | Tiling 等可落在 **op_host**（图编译 / `TilingFunc`），与 KernelLaunch 分工不同 |

本项目研读 MatmulLeakyRelu 时，以 **KernelLaunch** 为准；勿把 FrameworkLaunch 的 Host tiling 误套到 KernelLaunch 上。

### 6.1 职责划分（纠正：tiling 整块在 Scalar；Host 仅应用壳）

| 层级 | 典型文件 | 运行位置 | 职责 |
|------|----------|----------|------|
| **应用壳（非算子本体，用户确认）** | `main.cpp`、`data_utils.h` | **Host CPU** | **只是应用壳**：bin I/O、`aclrtMemcpy` 灌 GM、`ICPU_RUN_KF` / `ACLRT_LAUNCH_KERNEL`、同步与回收。**不承担** 任何算子算法（含 tiling 生成、分块、矩阵、激活） |
| **Scalar 标量** | `*_tiling.cpp` **整文件** + `*_custom.cpp` 内 `__aicore__` 普通 C | **AI Core — Scalar 单元** | **全部 tiling 相关计算与操作**：`MultiCoreMatmulTiling` / `GetTiling` / `SaveToBuffer`（`*_tiling.cpp`）；kernel 内 `CopyTiling`、`CalcOffset`、`Ceiling`、`Process` 循环、`CopyOut` 偏移。**普通 C/C++** 可写在 AscendC 工程 `.cpp` 中（kernel 侧加 `__aicore__`） |
| **Cube 矩阵** | `*_custom.cpp` 内 `Matmul<>` 等 | **AI Core — AIC** | `SetTensorA/B`、`SetBias`、`Iterate`、`GetTensorC` |
| **Vector 矢量** | `*_custom.cpp` 内高阶向量 API | **AI Core — AIV** | 当前 tile 的 `LocalTensor` 逐元素运算（样例为 `LeakyRelu`，可替换） |
| **搬运** | `DataCopy` 等 | **搬运单元** | tile 末端写 GM；Cube↔Vector 中间不经 GM |

**原则**：KernelLaunch 下 **Scalar / Cube / Vector 三分算子本体**；Host 只 **launch + I/O + 灌数**。

**样例工程接线说明**：`main.cpp` 里调用 `GenerateTiling()` 并把结果 `memcpy` 到 GM，是 **把 Scalar 侧 tiling 逻辑挂到可执行程序上的调测接线**，便于 `cpu/sim/npu` 跑通；**语义上** `matmul_leakyrelu_custom_tiling.cpp` 仍是 **算子 tiling 实现**（README 亦称其为「算子 tiling 实现」），归属 **Scalar 职责**，不是「Host 在算 tiling」。

**Scalar 范本（与 LeakyRelu 无关，可复用）**——`matmul_leakyrelu_custom.cpp` 第 15–18 行：

```cpp
__aicore__ inline uint32_t Ceiling(uint32_t a, uint32_t b)
{
    return (a + b - 1) / b;
}
```

`CalcOffset` 内用 `Ceiling(tiling.M, tiling.singleCoreM)` 做分核网格计算；换任何算子，只要调度逻辑需要向上取整，都可在同一 AscendC 源文件里用同样方式写标量 C 函数。

### 6.2 工程文件拆分（KernelLaunch 范式）

```text
├── *_tiling.cpp    # Scalar：GenerateTiling() — MultiCoreMatmulTiling / GetTiling / SaveToBuffer
├── *_custom.cpp    # Scalar 调度 + Cube + Vector：__global__ __aicore__ 入口与 Kernel 类
├── main.cpp        # 应用壳：灌 GM、launch kernel（无算法计算）
├── data_utils.h    # 应用壳：bin I/O
└── run.sh          # cpu / sim / npu 一键编译运行
```

与本项目 **Golden I/O + bin 布局** 一致：bin 由壳准备；**tiling 的生成、解析、消费均在 Scalar 语义下完成**（生成逻辑在 `*_tiling.cpp`，kernel 内 `CopyTiling` 从 GM 读入）。

### 6.3 Kernel 类骨架（可复用模板）

```text
class XxxKernel {
  Init(...)           // GlobalTensor 绑定 + 按 blockIdx 偏移 + TPipe/TQue InitBuffer
  Process(pipe)       // while (高阶对象.Iterate<sync>()) { PhaseA(); PhaseB(); CopyOut(); }
  PhaseA()            // Cube：GetTensorC → LocalTensor
  PhaseB()            // Vector：高阶 API 或逐元素，in-place 可 src==dst
  CopyOut(count)      // DataCopy(Local → GM[offset])；释放 queue
  CalcOffset(...)     // Scalar：多核分块算术（普通 C，如 Ceiling）
  Ceiling(a,b)        // Scalar：可复用辅助函数，与具体 Vector 算子无关
};

extern "C" __global__ __aicore__ void xxx_custom(...) {
  TPipe pipe;
  CopyTiling(&tiling, tilingGm);
  XxxKernel k; k.Init(...);
  REGIST_MATMUL_OBJ(&pipe, GetSysWorkSpacePtr(), k.matmulObj, &k.tiling);  // 用 Matmul 时
  k.Process(&pipe);
}
```

**要点**：

- 入口函数尽量薄：建 `TPipe`、拷 tiling、注册高阶对象、`Process`。
- **一 tile 一迭代**：`Iterate` 推进一步，内层 `PhaseA → PhaseB → CopyOut` 成流水线，避免整图一次拉满 UB。

### 6.4 Cube→Vector 融合的关键写法（无显式 GM 往返）

1. **在 `Matmul<>` 模板里把 C 的 `TPosition` 设为 `VECIN`**（而非 GM）  
   → 声明矩阵乘结果进入 Vector 侧缓冲区，而不是先落 GM。

2. **`GetTensorC<sync>(local, …)`** 把当前 tile 写入 `LocalTensor`（通常从 `TQue` `AllocTensor` 得来）。

3. **`Iterate<true>` / `GetTensorC<true>`** 的 `true`：打开 Cube 与后续阶段之间的同步，由框架/高阶 API 协调 AIC↔AIV，应用层**不必**手写 `CrossCoreSetFlag`（除非下沉到手写 MIX）。

4. **`REGIST_MATMUL_OBJ(&pipe, workspace, matmulObj, &tiling)`**：把 Matmul 对象挂到 `TPipe`，统一 workspace 与流水。

5. **`TQue<QuePosition::VECOUT, depth>`**：`AllocTensor` → Vector 算子 → `EnQue` → `CopyOut` 里 `DeQue`/`FreeTensor`，管理 UB 生命周期。

**数据流（tile 级）**：

```text
GM(A,B,…) → [Cube] Iterate/GetTensorC → LocalTensor(VECIN 语义)
         → [Vector] 逐元素 API(标量参数可传 slope/q 等)
         → DataCopy → GM(输出 tile)
```

### 6.5 Tiling：整块归属 Scalar（`*_tiling.cpp` + kernel 内消费）

**Cube:Vector = 1:2**：`SetDim` 填 **Vector 数**（样例为 2），`main` 里 `blockDim` 填 **AI Core 数**（样例为 1）；`baseM`/`baseN` 与 `singleCoreM`/`singleCoreN`/`singleCoreK` 须与此配比一致，详见 **§7.5**。

| 环节 | 文件 / 函数 | Scalar 上做什么 |
|------|-------------|-----------------|
| **生成** | `matmul_leakyrelu_custom_tiling.cpp` → `GenerateTiling()` | `SetDim`、`SetFixSplit(baseM,baseN)`、`SetOrgShape/SetShape`、`GetTiling`、`set_stepM/N`、`SaveToBuffer` |
| **注入 GM** | 应用壳 `main.cpp` | 仅 `memcpy` / `aclrtMemcpy` 把 tiling buffer 放到 `tilingGm`（**搬运，非计算**） |
| **读回与调度** | `*_custom.cpp` → `CopyTiling`、`CalcOffset`、`CopyOut` | 从 GM 解析 tiling；按 `GetBlockIdx()` 算子矩阵偏移；按 `count` 算写回偏移 |

- **`*_tiling.cpp` 与 kernel 内 tiling 消费同属 Scalar 流水线**，不要拆成「Host 算 tiling、核上只用结果」。
- `Ceiling` 等辅助函数与 `GenerateTiling` 中的 `M/N/K/baseM` 决策一样，都是 **可用普通 C 表达的标量逻辑**。
- `workspace = userWorkspace + GetLibApiWorkSpaceSize()` 由应用壳分配 GM 大小；高阶 Matmul 需系统 workspace。

### 6.6 多核并行（Scalar 调度 + Cube/Vector 计算）

- Launch `blockDim` = **AI Core 个数**（本工程仅 **Atlas A2**，见 §7.5；勿与 `SetDim` 的 Vector 数混淆）。
- 每核 **`GetBlockIdx()`**（Scalar 可读系统变量）+ **`CalcOffset`**（Scalar 普通 C）→ 本核 A/B/C 在 GM 中的偏移。
- `Ceiling`、`mCoreIndx`/`nCoreIndx` 等均为 **Scalar 标量逻辑**，与后续 Vector 算子种类无关。

### 6.7 对本项目（ML-KEM / PQC）的映射提示

| 范本环节 | 可映射到 KeyGen / NTT 原型 |
|----------|---------------------------|
| **Scalar — `*_tiling.cpp`** | NTT 批大小、系数条数、多核切分、`k=4` 子问题布局（Tiling API，与 Matmul 范本同构） |
| **Scalar — `*_custom.cpp` 调度** | `CopyTiling`、蝶形/块级索引与偏移、轮次、`Ceiling` 类函数（**普通 C**） |
| Cube / Matmul | Stage2 矩阵化等；或不用 Matmul 高阶类 |
| Vector 逐元素 | 蝶形、逐点乘、lazy sum、模约减（Stage3.1） |
| **应用壳 main** | Golden bin 灌 GM、launch、收结果；**不算子算法** |
| 其他 Host 服务（若需要） | SHA3、encode/decode 等与 KernelLaunch 算子本体分离，走独立 Host 路径 |
| tile 循环 + CopyOut | 分块进 UB，算完一块写 GM 一块 |

**KernelLaunch 默认骨架**：**Scalar（tiling 生成 + 解析 + 调度）→ Cube / Vector 分 phase → 末端 DataCopy 写 GM**；Host 不参与这三段中的任何一段。

### 6.8 与 §二、FrameworkLaunch 的衔接

- §二：架构上 Cube 结果可不经 GM 直达 Vector；§6.4：`TPosition::VECIN` + `Iterate/GetTensorC` + `TQue/TPipe`。
- 同仓库 **FrameworkLaunch** 下 `op_host/TilingFunc` 属于 **另一范式**（图模式 Host tiling）；本项目若做 **KernelLaunch 型** NTT/KeyGen 原型，以 §6 Scalar 整块 tiling 为准。

---

## 七、AscendC 与 ntt_study 差异 · 三段式 NTT 实现备忘（用户研究，**方案未定**）

> 以下为用户已做的 AscendC / 昇腾 NPU 研究结论，以及以 **MatmulLeakyReluInvocation** 为模板实现 **ntt_study 三段式 NTT** 时的**备选信息**。**此刻不锁定**具体 API 路线；实现时再选型，并记入 `docs/specs/` / PDF 查阅索引。

### 7.1 与 ntt_study 设备路径的差异（能力假设）

| 主题 | ntt_study 交付/旧设备习惯 | 本仓 AscendC 9.0（用户判断） |
|------|---------------------------|------------------------------|
| Stage2 矩阵乘 | 常走 **fp16×fp16→fp32**（迁就当时 GEMM） | **Cube 支持 int8 入、int32 出**，可直接 **int8 矩阵乘→int32**，不必为矩阵计算转浮点 |
| Stage3.1 取模 | 旧环境 Div 不可靠 → ONNX 图 **双校正**、避开 Mod | AscendC **除法 API 计算应正确**；不必默认照搬 ntt_study 双校正 |
| Stage1 编码 | 常转 half 再进矩阵 | 可用 **位运算**（与/或/非、算术/逻辑位移）；**注意各 API 支持的输入输出 dtype 组合** |
| 模运算（高阶） | — | 高阶 API 有 **Fmod**（**half、float** 上的模运算，PDF §2.4.1.30） |

**仍复用 ntt_study**：k=4、n=256、q=3329、limb6 表语义、**golden I/O**（如 `stage123_halfin_fp32out_stage31mod_f203ntt` 的 `input*.bin` / `golden.bin`）。**不要求**中间 tensor 与 ONNX 交付图 **同 dtype、同构图**。

### 7.2 以 MatmulLeakyRelu KernelLaunch 为模板的 NTT 方向（未开工，仅备忘）

- 目录范式：`exp-*` 下 `*_tiling.cpp` + `*_custom.cpp` + **main 应用壳**（§四、§6）。
- `Process` 相位由「Matmul + LeakyRelu」改为 **Stage1 编码 → Stage2 Matmul → Stage3 合并 → 取模**（相位名与是否拆多个 kernel 待定）。
- Stage2 范本替换点：`Matmul<…, int8, int8, int32, …>` + `TPosition::VECIN` 等，对齐 MatmulLeakyRelu 的 Cube→Vector 衔接方式。
- 分阶段验收思路（P1–P4 子步、先 Stage2 等）见对话纪要；**实施顺序未拍板**。

### 7.3 实现时的多方案空间（**不定 A/B/C**，仅供选型参考）

同一语义（尤其 **mod q**）在 AscendC 上可有多种合法实现，按 **dtype 支持位数、UB、可读性、验收节奏** 选用：

| 代号 | 思路 | 备注 |
|------|------|------|
| **A** | `Cast` → **`Fmod`** → `Cast` | 在 Fmod 支持的 **half/float** 范围内；q 与中间值需点验数值 |
| **B** | **向量 API 拼接**（如 Div/Mul/Sub 等） | 整数语义清晰；用户认为 AscendC Div 可信 |
| **C** | **Scalar 嵌 C**：`for` 循环 + **`%` 取模** | 写在 `__aicore__` / `*_custom.cpp`；小规模（如 k=4×256）功能首版可行 |

**通则**（不限于 mod）：语义目标 → 优先查 Vector/高阶 API 的 **dtype 表** → 可 API 拼接 → 可 **普通 C 循环** 作 fallback；**均落在 AI Core 算子本体**，不是 Host。

实现选定某条路径后，在 spec / 查阅索引记：**语义 + 选用方案 + dtype 约束**（便于日后替换方案不与 golden 口径冲突）。

### 7.4 查阅索引待补（实现时再写）

- Matmul **int8 / int32** `SetCType` 与 Cube 样例页码  
- **BitwiseAnd/Or/Not、ShiftLeft/ShiftRight** dtype 约束（PDF §2.3.3.2、§2.4.1.44–47）  
- **Fmod** §2.4.1.30（p.1405 起）

### 7.5 MIX 算子 tiling：`SetDim` 是 Vector 数，`blockDim` 是 AI Core 数（用户提示）

MatmulLeakyRelu 样例里两处数字**含义不同**，混用会导致 **tiling 错误**：

| 代码位置 | 变量 | 含义（用户说明） | 示例 |
|----------|------|------------------|------|
| `*_tiling.cpp` | `usedCoreNum` → `SetDim(...)` | 参与 Matmul 的 **Vector 总数**（AIV） | `blockDim=1` → **2**；`blockDim=4` → **8** |
| `main.cpp` | `blockDim` | Launch 的 **AI Core 个数**（AIC） | **1** 或 **4** 等 |

**本工程当前阶段（Atlas A2）**：**`usedCoreNum = blockDim × 2`**。每 AIC 内 **1×Cube + 2×AIV**；`SetDim` 填的是本次 launch 涉及的 **全部 AIV 数**，不是「每个 AIC 固定写 2、与 blockDim 无关」。存在 `usedCoreNum ≠ blockDim×2` 的反例，但不在 samples 内、本阶段暂不考虑。

```text
blockDim = 1  →  SetDim(2)    # 1 AIC × 2 AIV
blockDim = 4  →  SetDim(8)    # 4 AIC × 2 AIV
```

**`baseM` / `baseN` / `SetFixSplit`** 以及 `GetTiling` 产出的 **`singleCoreM` / `singleCoreN` / `singleCoreK`** 须满足 Cube:Vector 配比；**不能把 `SetDim` 理解成 AI Core 数**。多 AIC 时另需 `SetSingleShape` 等声明每 AIC 子块，并与 `CalcGMOffset(GetBlockIdx())` 一致。

读 MatmulLeakyRelu / `ascendc-tests/int8-matmul-cube-128x512x512` 时以 **Atlas A2** 为准。多 NPU 多 Core 扩展见 §一（8 NPU × 20 Core）。

### 7.6 int8-matmul 多核实验与规范（2026-06-11）

- **探针**：`ascendc-tests/frozen/frozen-int8-matmul-cube-128x512x512/`
- **实验纪要**：[2026-06-11-…#exp-int8-matmul](2026-06-11-ascendc-engineering-notes与数据搬运.md#exp-int8-matmul-多核-tiling-实验)（失败模式、`SetFixSplit(16,32,-1)`、`SetSingleShape` 三种闭合写法）。
- **定稿公式**：$(M/\text{SingleCoreM})\times(N/\text{SingleCoreN})=\text{usedCoreNum}$；`SingleCoreK=K`；`baseM`/`baseN` 为 16/32 倍数。
- **规范 PDF**：[融合算子多核tiling策略指南.pdf](../../docs/specs/ascendc/融合算子多核tiling策略指南.pdf)（`docs/specs/ascendc/`）。

---

## 八、exp-sepolyvec8-ntt-k8 用例方案（2026-06-09，仅文档无代码）

| 项 | 说明 |
|----|------|
| 目录 | `examples/incubating/ml-kem/ml-kem-1024/exp-sepolyvec8-ntt-k8/` |
| 方案 PDF | [exp-sepolyvec8-ntt-k8-实现方案.pdf](../../examples/incubating/ml-kem/ml-kem-1024/exp-sepolyvec8-ntt-k8/exp-sepolyvec8-ntt-k8-实现方案.pdf)（同源 `.tex`） |
| 语义 | `sepolyvec8_ntt_f203`：ML-KEM-1024 $k=4$，8 路 $s\Vert e$ 批 NTT |
| 模板 | MatmulLeakyRelu **KernelLaunch**；int8→int32 Cube；mod **向量拼接**（不用 Fmod/`%`） |
| 首版核数 | **1 AI Core**（`blockDim=1`） |
| int8 MatMul 最小 tile | **$16\times32\times16$**（用户补充，写入方案 §5.2） |
| Python 数据流 | **学 MatmulLeakyRelu**：`x1_gm.bin` 式 `*_gm.bin` 命名；`main.cpp` `ReadFile` 路径字面量匹配；每次 `gen_data`→kernel→`verify_result`（**cpu/sim/npu 均对拍**）；**非** ntt_study `input0/1.bin`（见方案 §5、§8） |
