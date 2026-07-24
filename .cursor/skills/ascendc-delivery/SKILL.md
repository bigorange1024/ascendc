# ascendc-delivery — 定型 / stable / 交付验收

## 何时使用

- 任务含 **`#关键词#`**（左右各一个 `#`）：`#交付#` `#验收#` `#完成#` `#原型#` `#新写#` `#修改#` 等。
- 或用户点名「按 ascendc-delivery skill」。

## 门禁 0：customspec（先于 examples 交付代码）

在 **`examples/`**（含 `examples/stable/`）内 **新增 / 修改 / 删除** 交付类代码之前，**必须**有用户指定的 `*-customspec.*` 规格说明书路径，且交付实现不得偏离该 spec。

**若无指定 customspec：**

- **禁止**写/改/删 `examples/` 下交付代码。
- **只能**：执行 **ascendc-impl-spec** 新建 customspec，或请用户指明路径。
- **即使用户催促**，也不得在未指明 customspec 时写码（回复模板同 **pre-research**「门禁：customspec」）。

**不受本门禁约束：** `src/`、`thirdparty/`、`include/` 等 examples 外目录。

## 目标

研究方案已定；在 **`examples/stable/stable-*`** 做可重复验收（基准 I/O + AscendC + `run.sh`）。

## 开任务前

1. 已读根 `README.md`、`qa/INDEX.md`、`.cursor/rules/ascendc-development.mdc`。
2. 已读 `examples/INDEX.md`、`examples/stable/INDEX.md`（若存在 stable 算子）。
3. 已确认用户指定的 **`*-customspec.*`** 路径（若要在 `examples/` 写码）。

## 写码前必读（强制）

在 **`examples/`**（含 `stable/`）或 **`ascendc-tests/`** 内写/改交付类实现之前，**必须先阅读**：

**[ascendc-engineering-notes/SKILL.md](../ascendc-engineering-notes/SKILL.md)**（AscendC **平台通则**，强制）

场景路线纪要（非强制）：[route-and-scenario-notes.md](../ascendc-engineering-notes/references/route-and-scenario-notes.md)。

（与 Rule、customspec、baseline-registry 并列；不得跳过 SKILL。）

## 门禁（先于写码）

1. 存在用户指定的 **`*-customspec.*`**，且 Agent 已读（**主规格**；实现不得偏离）。
2. 存在对应 **`exp-*`**，或用户明确指定复制来源；**不得在 stable 空写首版**。
3. **baseline-registry 硬卡点（强制）**：在用户准备执行 **`#交付#` / `#验收#`**（即将 `exp-*` **复制晋级** `stable-*`）**之前**，必须已定稿  
   **`docs/specs/<主题>-baseline-registry.md`**，并已登记进 [`docs/specs/INDEX.md`](../../docs/specs/INDEX.md)。  
   - **此前**（`$规格$`、【预研】写码、CPU/SIM/KAT）允许缺表或仅草稿。  
   - **一到晋级**：无定稿 registry → **禁止**复制进 `stable/`；停下补表或请用户确认豁免。  
   - 表内计算块均须有**已验证**来源；golden / KAT **仅**可调用表内 API/LUT。
4. **缺项 → 停**，提示用户补来源或回 **pre-research** / **ascendc-impl-spec**。
5. 基准侧：**仅**调用登记表 API/LUT 生成 I/O；**禁止**重造 NTT 等核心。
6. AscendC 侧：按 **customspec + specs / 讨论 / 标准** 实现；**禁止**把基准源码当实现模板。

## 目录与版本

| 操作 | 做法 |
|------|------|
| **晋级** | **复制** `exp-*` → `stable-<简述>-v1`（或首版 `stable-<简述>/`）；`exp-*` **不变** |
| **修订** | **复制** → `stable-<简述>-vN`；旧版在 `examples/stable/INDEX.md` 标 **已取代** |
| **索引** | 刷新 `examples/stable/INDEX.md`、`examples/INDEX.md` |

动手前若涉及新版本 stable，须用户确认 **vN**、复制来源与 **customspec 路径**。

## 交付测试流水线

```text
登记表约束的基准 → input/*.bin + golden.bin
→ AscendC（按 customspec，I/O 对齐）
→ run.sh（cpu / sim-build / SIM_DIRECT sim …）
→ cmp / [SUCCESS] matches golden
→ 按任务声明的验证阶梯级别 N 汇报（五档状态 + 证据）
```

- 多阶段：子链通过后仍须**全链再验**。
- `ascendc-tests/add_custom/` 为工具链冒烟参考；交付算子在 `stable-*`。

## 结束检查清单

- [ ] **`docs/specs/<主题>-baseline-registry.md` 已定稿**且已进 `docs/specs/INDEX.md`（晋级前硬卡点；见上「门禁」§3）
- [ ] `RUN.md` 与 `run.sh` 可复现
- [ ] **CPU + `SIM_DIRECT=1` sim 均已通过**；SIM dump 仅在 `OPPROF_*/dump/`，用例根无 stray `core*.dump` / `profile_*_log*.toml`（Rule「Agent 跑用例验收」）
- [ ] 自研代码中文注释（Rule「自研代码：中文注释」）：AscendC/Python/C 文件头 + 函数头 + 函数体内该注尽注；含 `scripts/gen_data.py`、`verify_result.py`
- [ ] 红旗自查：无「移植自」措辞、无逐步对应参考实现
- [ ] 更新 `examples/stable/INDEX.md`、`examples/INDEX.md`
- [ ] stable `STATUS.md` **链到**对应 baseline-registry
- [ ] 若变更基准来源，更新 `docs/specs/*-baseline-registry.md`
- [ ] 关键决策写入**当日** `qa/YYYY-MM/YYYY-MM-DD-….md`；同步 `qa/INDEX.md`、`qa/TODO.md`

## 测试在本 Skill 下

带 **`#…#`** 的测试（如 `#验收# run.sh cpu`）→ 本 Skill。
