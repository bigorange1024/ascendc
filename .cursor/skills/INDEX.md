# Cursor Skills — 场景操作手册索引

**位置**：仅 `.cursor/skills/`（仓库根**无** `skills/` 目录）。

**变更**：修改本目录下任意 `SKILL.md` **须用户当次任务明确确认**（见 Rule「Rule 与 Skill 变更」）。

---

## Skill 一览

| Skill | 目录 | 何时激活 |
|-------|------|----------|
| **ascendc-engineering-notes** | [ascendc-engineering-notes/SKILL.md](ascendc-engineering-notes/SKILL.md) | **不依赖触发词**；**写码前强制阅读**（平台约束 + **§8.1 会话排程**：先 CPU 成批 SIM、交叉前移） |
| *(可选)* 场景路线纪要 | [ascendc-engineering-notes/references/route-and-scenario-notes.md](ascendc-engineering-notes/references/route-and-scenario-notes.md) | **非强制**；NTT / merged_kyber 等；**MLKEM Tag5T 定稿**见 [docs/notes/MLKEM-NTT-实现总结.md](../../docs/notes/MLKEM-NTT-实现总结.md) |
| **ascendc-impl-spec** | [ascendc-impl-spec/SKILL.md](ascendc-impl-spec/SKILL.md) | `$方案$` `$写方案$` `$计划$` `$写计划$` `$规格$` `$写规格$` 等 `$…$`；产出 `*-customspec.*`（须写清计划 **AI Core** 数：1 / 多 / 多档验证） |
| **pre-research** | [pre-research/SKILL.md](pre-research/SKILL.md) | `【预研】` `【调研】` `【实验】` `【迭代】` 等；**且**用户已指明 `*-customspec.*` 才可动 `examples/` 代码 |
| **ascendc-delivery** | [ascendc-delivery/SKILL.md](ascendc-delivery/SKILL.md) | `#交付#` `#验收#` `#修改#` 等（`#关键词#`）；**且**用户已指明 `*-customspec.*` 才可动 `examples/` 代码 |

### 昇腾官方 Skill（精选 vendored）

见 [vendor/README.md](vendor/README.md)。来源 [Ascend/agent-skills](https://github.com/Ascend/agent-skills)。

| 目录 | 用途 |
|------|------|
| [vendor/cann-operator-env-config](vendor/cann-operator-env-config/SKILL.md) | CANN 安装与环境 |
| [vendor/npu-smi](vendor/npu-smi/SKILL.md) | NPU 设备管理 |
| [vendor/ascend-profiling-anomaly](vendor/ascend-profiling-anomaly/SKILL.md) | Profiling / 瓶颈 |

---

## 写码加载顺序（pre-research / ascendc-delivery）

1. `.cursor/rules/ascendc-development.mdc`
2. **ascendc-engineering-notes**（本仓经验）
3. 用户指定的 `*-customspec.*`（仅 `examples/`）
4. 场景 Skill 正文（pre-research 或 ascendc-delivery）

---

## customspec 样例

| 用例 | 路径 |
|------|------|
| F203 Stage1 纯向量 | `examples/incubating/exp-fips203-mlkem-pke-stage1-encode-vec/exp-fips203-mlkem-pke-stage1-encode-vec-实现方案-customspec.tex` |
| F203 Stage3 预研 | `examples/incubating/exp-fips203-mlkem-pke-stage3-routea-mod-vec/exp-fips203-mlkem-pke-stage3-routea-mod-vec-实现方案-customspec.tex` |
| F203 批 NTT k=8 | `examples/incubating/exp-sepolyvec8-ntt-k8/exp-sepolyvec8-ntt-k8-实现方案.tex`（规格书；实现已对齐 poly8 探针） |

## 冲突

| 情况 | 处理 |
|------|------|
| 同一句 **【】** 与 **`#…#`** | **告警、禁止操作**，请用户指定 Skill |
| **`$…$`** 与 **【】** | 默认 **先 customspec（ascendc-impl-spec），再预研写码** |
| **【预研】/ `#交付#` 写 examples 代码但无 customspec 路径** | **禁止写码**；索要路径或 `$写方案$` |

## 与 Rule 关系

本目录为**流程与检查清单**；恒定底线见 `.cursor/rules/ascendc-development.mdc`。

**`ascendc-tests/`**：平台功能探针；**不适用** customspec、**不**晋级 stable；子目录**无** `exp-` 前缀。见 `ascendc-tests/INDEX.md`。

**`examples/` 外**（含 `ascendc-tests/`、`src/`、`thirdparty/` 等）：不受 `examples/` customspec 门禁。
