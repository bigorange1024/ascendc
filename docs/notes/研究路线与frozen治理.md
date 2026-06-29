# 研究路线与 frozen 治理

**读者**：本仓库全体 Agent 与协作者  
**目的**：说明 **研究型仓库如何管理「已判决关闭的路线」**，而非罗列 frozen 目录名  
**Rule 条文**：[ascendc-development.mdc](../../.cursor/rules/ascendc-development.mdc) §`**/frozen/`**

**一句话**：**进门读关闭说明，出门不带码。**

---

## 0. 本文怎么读

| 章节 | 内容 |
|------|------|
| §1 | 研究型仓库的正常形态（为何 frozen 会越来越多） |
| §2 | frozen 的语义：判决书 vs 代码库 |
| §3 | Agent 写码流程 |
| §4 | 关闭新路线时的操作清单 |
| §5 | 模式：典型犯规与正确做法 |
| §6 | 附录：两条 frozen 树与案例 |

---

## 1. 研究型仓库的正常形态

| 现象 | 含义 |
|------|------|
| 探针、`exp-*` **很多** | 调研期试错，正常 |
| 多数目录 **不再维护** | 路线被证伪、取代或跑不通，正常 |
| 关闭路线迁入 `**/frozen/`** | **判决关闭**，不是「做好给大家抄」 |
| 活跃 `INDEX.md` 只剩少数项 | 迭代后的 **当前真相** |

**推论**：frozen 增多、活跃项减少是 **预期终态**，不是清理遗漏。

---

## 2. frozen 是什么、不是什么

### 2.1 语义

| frozen **是** | frozen **不是** |
|---------------|-----------------|
| 路线的 **死刑判决书**（+ 留档源码） | 实现参考库、snippet 仓库 |
| `FROZEN.md` 中「为何不采纳」 | 「抄一段就能用」的备选 |
| 防止 **重复踩坑** 的索引 | 历史权威探针、fork 源 |

### 2.2 进门（允许）

- 读 `FROZEN.md`、`STATUS.md`、`frozen/INDEX.md`、`qa/` 纪要  
- 弄清：**为何关闭**、**继任活跃路径**  
- 搜索命中 frozen 时以判决书为准  

### 2.3 出门（禁止）

- 复制 / 移植 frozen 内源码、数据面、FSM、类名、customspec 到活跃目录  
- 文档写「参考 frozen-xxx 实现」「从 frozen 移植」  
- 以 frozen `.hpp/.cpp` 为 **编码模板**（判决书已说明别走这条路）

---

## 3. Agent 写码流程

```text
1. ascendc-tests/INDEX.md、examples/incubating/INDEX.md  → 活跃基线
2. docs/notes/、docs/specs/  → 定稿契约（原理层）
3. 任务涉及旧路线 → 读 FROZEN.md  → 记「不采纳 + 继任」→ 回活跃目录写码
4. 仍不够 → 问用户或 qa/；不从 frozen 源码「抠」实现
```

**技术总结写法**：原理优先，见 [技术总结写作模板.md](技术总结写作模板.md)。

---

## 4. 关闭新路线时的清单

1. 写 **`FROZEN.md`**：冻结原因、继任路径  
2. 更新 **`frozen/INDEX.md`**  
3. 从活跃 **`INDEX.md` 移除**  
4. 在 **`docs/notes/`** 写原理层总结（可选案例附录）；**`qa/`** 记决策过程  

---

## 5. 模式

### P-frozen-1：读判决，不抄码

**问题**：Agent 搜索到曾通过的 frozen 探针。  
**做法**：读 `FROZEN.md` → 实现只在活跃 INDEX 基线迭代。  
**禁忌**：把 frozen 类名/布局写进新代码。

### P-frozen-2：路线级废弃文档

整条技术路线否决时（如 NTT 内 `Matmul<>`），除 frozen 目录外应有 **原理说明**：  
[NTT-Matmul路线废弃说明.md](NTT-Matmul路线废弃说明.md)。

---

## 6. 附录

### 6.1 两条 frozen 树

| 路径 | 前缀 | 活跃继任 |
|------|------|----------|
| `ascendc-tests/frozen/` | `frozen-<原名>/` | `ascendc-tests/INDEX.md` 非 frozen 行 |
| `examples/frozen/` | `frozen-exp-<简述>/` | `examples/incubating/exp-*`、`stable-*` |

### 6.2 案例

| 犯规 | 正确 |
|------|------|
| 从 frozen 移植 `AivAlg13UbPipeline` | 读 FROZEN：se_pair 已关；在 **2s1e** 继续 |
| 搜索命中后把 frozen 类名写进新代码 | 读判决后退出；只在活跃 INDEX 迭代 |
| 「polybatch-s123 权威，抄 Stage3」 | 「已 frozen：Gather S3；继任 2s1e 平面 mat_c」 |

---

*2026-06-18：对齐 docs/notes 原理优先规范。*
