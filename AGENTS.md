# AGENTS.md — Coding agents（含 Cursor Cloud Agent）

> **给人看的总览**：[`README.md`](README.md)  
> **当日真相 / 下一任务**：[`AGENT_HANDOFF.md`](AGENT_HANDOFF.md)（**每日任务结束前必刷新**）  
> **详细底线**：[`.cursor/rules/ascendc-development.mdc`](.cursor/rules/ascendc-development.mdc)  
> **本文件角色**：Cloud / 任意 coding agent 的**短入口**；不复制长文，只给必读路径与硬门禁。

**最后刷新**：2026-07-14

---

## 1. 开任务顺序（强制）

1. [`README.md`](README.md) — 目标与顶层结构  
2. **本文件** `AGENTS.md`  
3. [`AGENT_HANDOFF.md`](AGENT_HANDOFF.md) — 当前真相与 P0  
4. [`qa/INDEX.md`](qa/INDEX.md) · [`qa/TODO.md`](qa/TODO.md)  
5. [`.cursor/rules/ascendc-development.mdc`](.cursor/rules/ascendc-development.mdc)  
6. 场景 Skill：[`.cursor/skills/INDEX.md`](.cursor/skills/INDEX.md)  
7. 写 AscendC 前：[`library/documents/CANN-AscendC算子开发接口参考-查阅索引.md`](library/documents/CANN-AscendC算子开发接口参考-查阅索引.md)

---

## 2. 仓库性质（一句话）

研究型 AscendC + PQC；**WSL / 无 NPU**，靠 **CPU 孪生 + CAModel SIM** 验收。活跃实现只认各 `INDEX.md`；`**/frozen/**` **可进读判决书，出门不带码**。

---

## 3. 硬门禁（违反即停）

| 项 | 要求 |
|----|------|
| `examples/` 写码 | 须有活跃 `*-customspec.*`（非 frozen）；`$…$`→规格，`【】`→预研，`#…#`→交付 |
| incubating / stable | 研究只写 `exp-*`；stable **只能从活跃 exp 复制晋级**；未压测绿 **禁止** 建/推 stable |
| frozen | 禁止把 `ascendc-tests/frozen/`、`examples/frozen/` 源码/customspec 抄进活跃树 |
| golden | 只验 **I/O 等价**（`run.sh` + cmp）；禁止以「与参考源码同构」验收 |
| AscendC API | 用前查查阅索引；无记录则查 PDF 并**同轮写回索引** |
| 自研注释 | Python/C/AscendC **与实现同轮**写详细中文注释 |
| 已锁参数 | 形状/tiling/`blockDim` 等不得擅自改参绕过；歧义先问用户 |
| Rule/Skill | `.cursor/rules/`、`.cursor/skills/` 变更须用户当次确认 |
| 验收声称 | 同用例目录须 **CPU + `SIM_DIRECT=1` sim** 双过；用例根无 stray dump |

Skill 符号冲突（同句 `【】` 与 `#…#`）→ **告警、禁止仓库操作**。

---

## 4. 环境与常用命令

```bash
# 建议先同步
git pull

# CANN 冒烟
bash ~/ascendc/scripts/verify-cann.sh

# 用例（在用例目录；默认即全量，勿手写与 default 相同的 HAT_*）
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

| 项 | 说明 |
|----|------|
| CANN | 社区版 **9.0.0**；常见路径 `~/Ascend/cann` |
| 并行 | WSL 建议 `CMAKE_BUILD_JOBS=2`；**勿并行多路 SIM** |
| `thirdparty/` | **不进 Git**；换机 `bash scripts/clone-thirdparty.sh` |
| Cloud VM | 可能无本机 CANN / NPU；缺环境时 **如实标阻塞**，勿假绿；可读文档与写规格仍可进行 |

完整复现：[docs/engineering/环境复现与开发指南.md](docs/engineering/环境复现与开发指南.md)

---

## 5. 文档分工（刷新时改对文件）

| 文件 | 谁维护 / 何时刷新 |
|------|-------------------|
| **`AGENTS.md`** | 开任务入口、硬门禁、文档地图变更时 |
| **`AGENT_HANDOFF.md`** | **每日**任务结束：当前真相 + 下一 P0（不堆历史） |
| **`README.md`** | 顶层目录/目标/当前状态表变更时 |
| **`qa/YYYY-MM/…`** | 当日决策与踩坑；同日只一篇 |
| **各 `INDEX.md`** | 增删目录或改活跃项时 |
| **`.cursor/rules/`** | 仅用户确认后 |

备份：`./backup-project.sh` 前须已刷新 INDEX / README / 本文件相关段落。

---

## 6. 当前主线（摘要；细节以 HANDOFF 为准）

- **PKE** 三段已在 `examples/stable/`  
- **KEM Alg.19 KeyGen**：incubating [`exp-fips203-mlkem-kem-keygen-k4`](examples/incubating/exp-fips203-mlkem-kem-keygen-k4/) 有条件完成；**stable 须用户 `#交付#` 后复制晋级**  
- 行为基线探针：`pass-fix-f203-alg19-kem-keygen-device-k4`（勿当 CMake 依赖）  
- 办公室常见下一刀：T19a Encaps device（见 HANDOFF）

---

## 7. 粘贴用短 Prompt

```text
先读 AGENTS.md、README.md、AGENT_HANDOFF.md、qa/INDEX.md 与 .cursor/rules/ascendc-development.mdc。
研究型工程：写码只认活跃 INDEX + docs/notes；frozen 读判决不抄码。
examples/ 须 active customspec；$…$→规格 【】→预研 #…#→交付。
验收须 CPU + SIM_DIRECT=1 sim；声称通过须有命令证据。
每日结束刷新 AGENT_HANDOFF.md；入口/门禁变了同步刷新 AGENTS.md。
```
