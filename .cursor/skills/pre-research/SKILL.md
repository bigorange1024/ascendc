# pre-research — 研究 / 预研 / 反复试验

## 何时使用

- 任务含 **`【关键词】`**：`【预研】` `【调研】` `【实验】` `【迭代】` 等（词表可扩展）。
- 或用户点名「按 pre-research skill」。

## 门禁：customspec（先于一切 examples 代码）

在 **`examples/`** 目录内 **新增 / 修改 / 删除** 预研类代码之前，**必须**同时满足：

1. 存在用户**明确指定路径**的规格说明书，文件名为 `*-customspec.*`（扩展名见 **ascendc-impl-spec**）。
2. Agent 已阅读该 customspec，且本轮改动在其范围内。

**若无指定 customspec：**

- **禁止**在 `examples/` 下写/改/删预研类代码（含 `exp-*` 内 kernel、CMake、`run.sh`、`scripts/`、用例头文件等）。
- **只能**：执行 **ascendc-impl-spec**（用户 `$写方案$` 等）新建 customspec；或**反复要求用户**给出已有 `*-customspec` 的**具体路径**。
- **即使用户催促或要求先写码**，也不得放弃本门禁；不得用口头描述、`qa` 讨论或旧版 `*-实现方案.tex`（无 `-customspec`）代替。

**固定回复模板（无 customspec 时）：**

```text
无法在 examples/ 下修改代码：未指定 *-customspec.* 规格说明书。

请二选一：
1) 给出已有规格书路径，例如：
   examples/incubating/exp-xxx/exp-xxx-实现方案-customspec.tex
2) 使用 $写方案$ 让我先在该用例目录生成 *-customspec.* 后再写码。

在您指明具体 customspec 文件之前，我不会在 examples/ 内增删改预研代码。
```

**不受本门禁约束：**

- `src/`、`thirdparty/`、`include/`、`docs/`、`qa/`、`library/` 等 **examples 以外**路径（用户要求即可改）。
- 仅更新 `examples/` 外文档，或仅有 customspec 后的 `STATUS.md` / INDEX 归档（不涉及内核代码时）。

## 目标

方案未定、多轮试、可能改路线。**不**声称 `examples/stable/` 交付完成。

## 开任务前

1. 已读根 `README.md`、`qa/INDEX.md`、`.cursor/rules/ascendc-development.mdc`。
2. 确认本任务属于**调研 / 预研**阶段，而非 stable 交付。
3. 已确认用户指定的 **`*-customspec.*`** 路径（若要在 `examples/` 写码）。

## 写码前必读（强制）

在 **`examples/`** 或 **`ascendc-tests/`** 内新增/修改 AscendC 内核、CMake、`run.sh`、`scripts/`、tiling 头文件等**可执行实现**之前，**必须先阅读**：

**[ascendc-engineering-notes/SKILL.md](../ascendc-engineering-notes/SKILL.md)**（AscendC **平台通则**，强制）

算子/路线专用纪要（如 NTT、merged_kyber）：**仅** customspec 或任务点名时读 [route-and-scenario-notes.md](../ascendc-engineering-notes/references/route-and-scenario-notes.md)。

（与 Rule、customspec 并列；不得跳过 SKILL。仅改 `docs/`/`qa/` 文档时可不读。）

## 代码与目录（强制）

**以下仅在 customspec 已指定后适用。**

- **只允许**在 **`examples/incubating/exp-<简述>/`** 写研究类代码。
- **禁止**：在 **`examples/stable/`** 直接新建或写入首版研究实现。
- **禁止**：跨 `exp-*` 覆写成另一方案（新方案须新建 `exp-*`）。
- 可配合 `src/` + `include/`（遵守单 `main`）、`docs/notes/`（定稿时）与 `qa/` 记录。

## 自研代码：中文注释（强制，与实现同轮）

凡在 `examples/` 内**新增或修改**的自研 **Python、C、AscendC**（含 `scripts/*.py`、kernel、`main.cpp`、`launch_profile.h` 等），须遵守 Rule **「自研代码：中文注释（强制）」**（`.cursor/rules/ascendc-development.mdc`）：

| 层级 | 要求 |
|------|------|
| **文件头** | 流水线位置、I/O 形状与 dtype、对齐的 customspec / golden / 交付引用 |
| **函数头** | 作用、入参/出参（含 GM 语义）、形状、前置条件 |
| **函数体内** | 循环算什么、索引含义、分支理由、tiling/同步点；**禁止**只有文件头、体内大段无说明 |

**注释与实现同轮**（Rule 写码门禁表）：与 kernel/scripts 同 PR / 同次提交写齐达到验收标准的注释；后续轮次可继续补充细化。验收：不查外部文档也能读懂每段在做什么。

## 动手前（必做）

向用户确认目录操作后再改 incubating 内代码：

```text
【目录操作确认】
customspec：<完整路径>
拟：新建 exp-<名> / 在既有 exp-<名> 迭代（同一方案？）/ 仅文档不写码
请确认。
```

## 工作方法

1. **先定边界**：I/O 约定、验证范围、风险、最小可跑命令（一条命令可复现）。
2. **每轮归档**（缺一则本轮无效）：
   - 输入来源
   - 执行的命令
   - 产物路径
   - 比对结果（或失败 log）
3. **局部通过 ≠ 全链通过**：子实验通过不能代替集成验收 claim。
4. **对照外部资料**：定稿原理写 `docs/notes/`（见写作模板）；过程写 `qa/`；不越权 claim「已交付」。

## 产出与归档

| 产出 | 位置 |
|------|------|
| **实现规格书** | `*-customspec.*`（见 **ascendc-impl-spec**；写码前须有） |
| **预研状态（必做）** | `examples/incubating/exp-<名>/STATUS.md`：状态词 + 验证命令 + 已达成/未达成 + 当前参数 + **customspec 路径** |
| **索引状态列** | `examples/incubating/INDEX.md` 同行：与 STATUS 一致的一行摘要 |
| 定稿技术总结 | `docs/notes/`（[技术总结写作模板.md](../../docs/notes/技术总结写作模板.md)） |
| 关键决策 | **当日** `qa/YYYY-MM/YYYY-MM-DD-<关键词>.md` + `qa/INDEX.md` 一行 |
| LaTeX customspec | `bash scripts/xelatex-clean.sh`（见 Rule「LaTeX / PDF」） |

**不**改 `examples/stable/INDEX.md`（未定型）。

预研任务**结束一轮**时，STATUS 与 incubating INDEX **必须**已更新；缺一则视为本轮归档未完成。

## 收敛 toward delivery

用户确认方案定型后，交由 **ascendc-delivery**：**复制** `exp-*` → `stable-<名>-v1`（或首版 `stable-<名>/`），`exp-*` **保留不动**。

## 测试在本 Skill 下

带 **【】** 的测试（如 `【实验】对拍`）→ 本 Skill；多用「有条件完成」等状态词 + 证据。
