# ascendc-impl-spec — AscendC 实现规格说明书（customspec）

## 何时使用

- 用户用 **`$文字$`** 括起意图，例如：`$方案$` `$写方案$` `$计划$` `$写计划$` `$规格$` `$写规格$`。
- 或用户明确要求：先写实现规格说明书，再写 AscendC 程序（预研 `exp-*` 或交付 `stable-*`）。
- 或点名「按 ascendc-impl-spec skill」。

**本 Skill 只负责产出 `*-customspec.*` 规格书**；在 `examples/` 内写码归 `pre-research` / `ascendc-delivery`，且须在 customspec 已存在并被用户指明之后。

## 写规格阶段：禁止写实现代码（强制）

在 **`#方案#` / `$写方案$` / ascendc-impl-spec 闭环完成之前**：

| 允许 | 禁止 |
|------|------|
| 新建用例目录下 **仅** `*-customspec.*`（及编译出的 `.pdf`） | kernel、`main.cpp`、CMake、`run.sh`、`scripts/*.py`、`launch_profile.h` 等**一切可执行实现** |
| 更新 `library/documents/…查阅索引.md`、`examples/incubating/INDEX.md`（仅登记规格书路径） | 以「顺手验证」为由写码、编译内核、跑 `run.sh` |
| 用户**明确说「可以写代码」**后，交由 pre-research / delivery | 用户未放行时，即使用户催促也**不得**写 `examples/` 内实现 |

**固定回复**（用户催写码但 customspec 未确认或未放行时）：

```text
当前处于 #方案# / customspec 阶段，按 ascendc-impl-spec 仅产出规格书。
实现代码须待您确认 customspec 并明确说「可以写代码」后再动手。
```

## 开任务前必读

1. `.cursor/rules/ascendc-development.mdc`（平台 A2、SIMD、LaTeX/PDF）
2. `library/documents/CANN-AscendC算子开发接口参考-查阅索引.md`
3. `library/offline-web/INDEX.md`
4. 上下文：当日 `qa/`、相关 `mlkem_ref.py` / 交付件 / 论文或标准段落
5. 相近样例：`samples/operator/ascendc/tutorials/*`、`examples/incubating/exp-*`

## 仓库样例（命名与内容）

| 用例 | customspec 路径 |
|------|-----------------|
| Stage1 纯向量（100% 向量覆盖） | `examples/incubating/exp-fips203-mlkem-pke-stage1-encode-vec/exp-fips203-mlkem-pke-stage1-encode-vec-实现方案-customspec.tex` |
| Stage3 预研（标量缺口标明） | `examples/incubating/exp-fips203-mlkem-pke-stage3-routea-mod-vec/exp-fips203-mlkem-pke-stage3-routea-mod-vec-实现方案-customspec.tex` |
| Stage1+2 融合（MIX，aicore=1；**已 frozen**） | `examples/frozen/frozen-exp-mlkem-f203-stage12-encode-matmul-mix/exp-mlkem-f203-stage12-encode-matmul-mix-实现方案-customspec.tex` |

推荐命名：`<exp 目录名>-实现方案-customspec.<ext>`，与用例同目录。

## 强制流程（按序，不得跳步）

### 1. 数学语义

- 写清 **输入/输出**（形状、dtype、值域、Host 前提）。
- 写清 **逐元素/逐阶段公式**（对齐 Golden、ONNX、交付件或标准，注明来源）。
- 标明 **本阶段不做** 的步骤（如 Stage1 不做 mod q）。

### 2. AscendC 实现规划

- 核类型：`KERNEL_TYPE_AIV_ONLY` / `KERNEL_TYPE_MIX_AIC_1_2` 等。
- **AI Core 规模（强制专节，不得省略）**：说明书须**单独写清**计划使用几个 **AI Core**，并与 `blockDim` / launch 配置一致。须写明：
  - **数量**：仅 **1** 个、仅 **多个**，或 **1 与多个均测**（列出各档 `blockDim` 及验证命令）。
  - **命名**：内部 `LAUNCH_PROFILE` 为 **`aicore=<N>`**（Cube/融合）/ **`aiv=<N>`**（纯 Vector）；`run.sh` 用 **`--aicore 1`**、**`--aiv 8`**（两参数，无需引号）。**不用** `1aic`、`--aic` 等旧写法。
  - **核角色**：纯向量（`AIV_ONLY` → Vector AI Core 数）、融合（`MIX` → 几个 AIC、几个 AIV，或等价的 1 AI Core 内 Cube:Vector 配比）。
  - **数据划分**：每个 AI Core 负责哪一段数据（如「1 核 1 poly」「4 核各 2 poly」）。
  - **选型理由**：若用户未口头指定核数，须在 spec 中说明为何选该规模（数据并行维、样例对齐、资源/tiling 约束等）；**禁止**实现里擅自改 `blockDim` 却不回写 spec。
- 并行：`blockDim`、每核数据划分、tile/双缓冲。
- 流水：CopyIn → Compute → CopyOut（或 Cube/Vector 融合阶段）。

**后续写码**（pre-research / delivery）须同步遵守 Rule **「自研代码：中文注释（强制）」**：kernel、`main.cpp`、`scripts/*.py` 等须文件头 + 函数头 + 函数体内分段中文说明（I/O、dtype/形状、讨论背景与结论），与实现同 PR / 同轮提交；后续可继续补注释。

### 3. 查资料（必须查，禁止凭记忆列 API）

| 来源 | 路径 |
|------|------|
| CANN 9.0 PDF | `library/documents/CANN社区版 9.0.0 Ascend C算子开发接口参考 01.pdf` |
| 离线网页 | `library/offline-web/` |
| API 索引 | `library/documents/CANN-AscendC算子开发接口参考-查阅索引.md` |
| 讨论 | `qa/YYYY-MM/` |
| A2 头文件交叉验证 | `$ASCEND_HOME/.../dav_c220/kernel_operator_*_impl.h` |
| 样例代码 | `samples/`、`examples/incubating/exp-*` |

### 4. 全量 API 表

每个拟用 API **一行**，字段齐全：

| 字段 | 要求 |
|------|------|
| API 名 | 如 `Cast`、`ShiftRight`、`DataCopy` |
| 分类 | PDF 章/节（2.3.1 搬运、2.3.3 矢量、资源管理等） |
| 在线文档 ID | `atlasascendc_api_07_*` 链接 |
| Atlas A2 | 是否支持（√/×） |
| 输入 dtype | 本用例实际类型 |
| 输出 dtype | 本用例实际类型 |
| 备注 | 约束、对齐、备选 Cast 链等 |

**类型转换**：若单次 `Cast` 不够，必须写主路径、**备选**、否决路径及原因。

**评估过但未采用的 API** 单独一小表。

### 5. 代入数学逐步评估覆盖率

| 数学步骤 | 选用 API | 覆盖 | 若未覆盖 |
|----------|----------|------|----------|
| … | … | 100% / 部分 / 0% | 再查 API **或** **标量**（写明位置与理由） |

主路径须给出是否 **100% 向量**；不能处必须显式标 **标量缺口**。

### 6. 产出物（强制）

- **文件名**：`*-customspec.<ext>`
- **允许扩展名**：`tex` `pdf` `txt` `md` `mdc` `doc` `docx` `ppt` `pptx` `xls` `xlsx` `xml`
- **位置**：与对应用例同目录（或用户指定目录）
- **文首元数据**：适用 `examples/...` 路径、I/O、日期、**计划 AI Core 数（及是否多档验证）**
- **若用 `.tex`**：同轮执行
  ```bash
  bash scripts/xelatex-clean.sh <path>-customspec.tex
  ```
  保留 `.tex` + `.pdf`，删除中间文件（见 Rule「LaTeX / PDF」）
- **`.tex` 长表排版（强制）**：凡含 API 名/长标识符的 `longtable`，**必须**与 `exp-fips203-mlkem-pke-stage1-encode-vec-*-customspec.tex` 一致：
  - `\usepackage{array}` + `\usepackage{seqsplit}`
  - `\newcolumntype{L}[1]{...}` 列宽 + `\newcommand{\apiname}[1]{\texttt{\seqsplit{#1}}}`；目录名用 `\dirname{exp-...}` 同样断行
  - **禁止**裸 `p{...}` 塞长 `\texttt{API1/API2/...}` 或长 `exp-*` 路径；每个 API **单独一行**
  - 两列表（用例↔语义）首列建议 `L{4.5cm}` 以上并 `\dirname`，避免目录名溢出压到第二列
  - 编译后目视 PDF，不得出现列文字重叠
- **更新** `library/documents/CANN-AscendC算子开发接口参考-查阅索引.md`（新增查阅记录）

### 7. 闭环后向用户回报

回报 **customspec 的完整仓库路径**；后续 `【预研】` / `#交付#` 写 `examples/` 代码须引用此路径。

## 与 pre-research / delivery 的衔接

- customspec 闭环后，用户 `【预研】实现` → 按 spec 写 `exp-*`（须用户指明 spec 路径）。
- 用户 `#交付#` → spec + `docs/specs/*-baseline-registry.md` 齐备后再晋级 `stable-*`。

## 禁止

- 未查文档就列 API 或 dtype
- 用 `*-实现方案.tex`（无 `-customspec`）作为新任务规格书文件名
- 裸跑 `xelatex` 不清理中间文件
- **customspec 阶段写 `examples/` 内实现代码**（须用户明确放行）
