# AGENTS.md — Coding agents（含 Cursor Cloud Agent）

> **给人看的总览**：[`README.md`](README.md)  
> **当日真相 / 下一任务**：[`AGENT_HANDOFF.md`](AGENT_HANDOFF.md)（**每日任务结束前必刷新**）  
> **详细底线**：[`.cursor/rules/ascendc-development.mdc`](.cursor/rules/ascendc-development.mdc)  
> **Cloud VM 环境细节**：[`Cursor-Cloud环境说明.md`](Cursor-Cloud环境说明.md)（非 WSL 启动/运行坑与 SIM 绕过）  
> **本文件角色**：Cloud / 任意 coding agent 的**短入口**；不复制长文，只给必读路径与硬门禁。

**最后刷新**：2026-07-14（`clone-thirdparty` 默认 build liboqs；Cloud SIM CANN 符号分轨）

---

## 1. 开任务顺序（强制）

1. [`README.md`](README.md) — 目标与目录结构  
2. **本文件** `AGENTS.md`  
3. [`AGENT_HANDOFF.md`](AGENT_HANDOFF.md) — 当前真相与 P0  
4. [`qa/INDEX.md`](qa/INDEX.md) · [`qa/TODO.md`](qa/TODO.md)  
5. [`.cursor/rules/ascendc-development.mdc`](.cursor/rules/ascendc-development.mdc)  
6. 场景 Skill：[`.cursor/skills/INDEX.md`](.cursor/skills/INDEX.md)  
7. 写 AscendC 前：[`library/documents/CANN-AscendC算子开发接口参考-查阅索引.md`](library/documents/CANN-AscendC算子开发接口参考-查阅索引.md)

**换机 / Cloud 首次**（在跑任何带 golden 的 `run.sh` 之前）：

```bash
bash scripts/clone-thirdparty.sh
# 默认会：clone 六仓 + build liboqs 0.15.0 + 编 liboqs_kem_ref/pke_ref
# 仅补编：bash scripts/build-liboqs.sh
# 只 clone 不编：BUILD_LIBOQS=0 bash scripts/clone-thirdparty.sh
```

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
| thirdparty | **先** `clone-thirdparty.sh`（含 liboqs **build**）；缺库时 golden/KAT 会挂 |

Skill 符号冲突（同句 `【】` 与 `#…#`）→ **告警、禁止仓库操作**。

---

## 4. 环境与常用命令

```bash
git pull
bash scripts/clone-thirdparty.sh          # 缺依赖则装；已装则 skip；默认 build liboqs
bash ~/ascendc/scripts/verify-cann.sh     # CANN 冒烟（Cloud 若无 CANN：标阻塞）

# 用例（在用例目录；默认即全量）
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

| 项 | 说明 |
|----|------|
| CANN | 社区版 **9.0.0**；常见路径 `~/Ascend/cann` |
| 并行 | 建议 `CMAKE_BUILD_JOBS=2` / `LIBOQS_JOBS=2`；**勿并行多路 SIM** |
| `thirdparty/` | **不进 Git**；权威 [`docs/engineering/thirdparty-本地依赖.md`](docs/engineering/thirdparty-本地依赖.md) |
| liboqs | tag **0.15.0**；[`scripts/build-liboqs.sh`](scripts/build-liboqs.sh)（`clone-thirdparty` 默认调用） |

### Cloud Agent 注意（分轨）

Cloud VM（非 WSL）的完整启动/运行坑与 SIM 绕过见 [`Cursor-Cloud环境说明.md`](Cursor-Cloud环境说明.md)。

| 现象 | 归属 | Agent 该怎么做 |
|------|------|----------------|
| `gen_data` / verify 缺 `liboqs` / `liboqs_kem_ref` | **thirdparty** | 跑 `bash scripts/clone-thirdparty.sh` 或 `bash scripts/build-liboqs.sh`；修好后重跑 **CPU** |
| `libge_common_base.so: undefined symbol: …InternalSwap…` / 启动即 `Floating point exception` | **dump 桩遮蔽真库 / DumpManager FPE**（与 liboqs 无关） | **已在 `scripts/sim_env.sh` 修复**（非 WSL 自动不装桩 + `CAMODEL_SKIP_ADX_WORK_PATH=1`）；标准 `run.sh -r sim` 直接跑。成因/覆盖开关见 [`Cursor-Cloud环境说明.md`](Cursor-Cloud环境说明.md) |
| Cloud 无 CANN | 环境 | 可读代码/写规格；**勿假绿** SIM |

完整复现：[docs/engineering/环境复现与开发指南.md](docs/engineering/环境复现与开发指南.md)

---

## 5. 文档分工（刷新时改对文件）

| 文件 | 谁维护 / 何时刷新 |
|------|-------------------|
| **`AGENTS.md`** | 开任务入口、硬门禁、文档地图、**Cloud 依赖步骤**变更时 |
| **`Cursor-Cloud环境说明.md`** | Cloud VM（非 WSL）启动/运行坑、SIM 绕过；随该环境变化刷新 |
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
换机/Cloud：先 bash scripts/clone-thirdparty.sh（默认 build liboqs）。
研究型工程：写码只认活跃 INDEX + docs/notes；frozen 读判决不抄码。
examples/ 须 active customspec；$…$→规格 【】→预研 #…#→交付。
验收须 CPU + SIM_DIRECT=1 sim；缺 CANN/SIM 符号异常须标阻塞勿假绿。
每日结束刷新 AGENT_HANDOFF.md；入口/门禁/依赖步骤变了同步刷新 AGENTS.md。
```
