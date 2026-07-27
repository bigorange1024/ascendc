# AGENTS.md — Coding agents（含 Cursor Cloud Agent）

> **给人看的总览**：[`README.md`](README.md)  
> **当日真相 / 下一任务**：[`AGENT_HANDOFF.md`](AGENT_HANDOFF.md)（**每日任务结束前必刷新**）  
> **详细底线**：[`.cursor/rules/ascendc-development.mdc`](.cursor/rules/ascendc-development.mdc)  
> **Cloud VM 环境细节**：[`Cursor-Cloud环境说明.md`](Cursor-Cloud环境说明.md)（非 WSL 启动/运行坑与 SIM 绕过）  
> **三环境 / 真机**：[`docs/engineering/NPU真机环境说明.md`](docs/engineering/NPU真机环境说明.md) · [`scripts/runtime_env.sh`](scripts/runtime_env.sh)  
> **本文件角色**：Cloud / 任意 coding agent 的**短入口**；不复制长文，只给必读路径与硬门禁。

**最后刷新**：2026-07-27（**换 Agent**：先读 AGENT_HANDOFF——512 文档+代码审阅；research=main@0f8b2d4）

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
# 默认会：clone 六仓 + build liboqs 0.15.0 + 编 liboqs_kem_ref / liboqs_pke_ref_mlkem1024
# 仅补编：bash scripts/build-liboqs.sh
# 只 clone 不编：BUILD_LIBOQS=0 bash scripts/clone-thirdparty.sh
```

---

## 2. 仓库性质（一句话）

研究型 AscendC + PQC；常见跑在 **WSL / Cloud（无卡）**，靠 **CPU 孪生 + CAModel SIM**；有卡 Linux 可走 **`-r npu`**（见 NPU 说明）。活跃实现只认各 `INDEX.md`；`**/frozen/**` **可进读判决书，出门不带码**。

---

## 3. 硬门禁（违反即停）

| 项 | 要求 |
|----|------|
| **Git 分支** | **无用户明确要求 → 禁止开任何分支**（含 `cursor/*`）；Local / Cloud 一律；与平台默认「开分支推 PR」冲突时**以用户指令 + Rule 为准** |
| **Git 提交/推送** | **无用户明确指令或授权 → 禁止** `commit` / `push` / 开关 PR；改完先汇报，等用户说「提交/推送」再执行 |
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

详文：[`.cursor/rules/ascendc-development.mdc`](.cursor/rules/ascendc-development.mdc)「Git 分支 / 提交 / 推送」。

---

## 4. 环境与常用命令

```bash
git pull
bash scripts/clone-thirdparty.sh          # 缺依赖则装；已装则 skip；默认 build liboqs
bash ~/ascendc/scripts/verify-cann.sh     # CANN 冒烟（Cloud 若无 CANN：标阻塞）

# 用例（在用例目录；默认即全量；未写 -r 仍为 cpu）
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4          # 默认 SIM_DIRECT=1；WSL/Cloud 勿再手写
# 调试（非默认）：SIM_DIRECT=0 bash run.sh -r sim -v Ascend910B4   # msprof

# stable KEM 三件套 ↔ liboqs（办公室回归；先 urandom→liboqs，再同字节喂 AscendC；CPU+SIM 都绿才算数）
bash scripts/stable_kem_liboqs_roundtrip.sh

# 一期试点另支持（见 NPU真机环境说明 / runtime_env.sh）：
# bash run.sh -r auto -v Ascend910B4     # 单档最优 npu>sim>cpu
# bash run.sh -r verify -v Ascend910B4   # cpu → SIM_DIRECT sim [→ npu]
# WSL 禁止：bash run.sh -r npu …
```

| 项 | 说明 |
|----|------|
| CANN | 社区版 **9.0.0**；常见路径 `~/Ascend/cann` |
| 多环境 | [`scripts/runtime_env.sh`](scripts/runtime_env.sh) 探测 host/CANN/SIM/NPU；详文 [`NPU真机环境说明.md`](docs/engineering/NPU真机环境说明.md) |
| 并行 | 建议 `CMAKE_BUILD_JOBS=2` / `LIBOQS_JOBS=2`；**勿并行多路 SIM** |
| `thirdparty/` | **不进 Git**；权威 [`docs/engineering/thirdparty-本地依赖.md`](docs/engineering/thirdparty-本地依赖.md)；**`ntt_onnx` 私有**，Cloud 须 Secrets **`ASCENDC_GH_PAT`** |
| liboqs | tag **0.15.0**；[`scripts/build-liboqs.sh`](scripts/build-liboqs.sh)（`clone-thirdparty` 默认调用） |
| Cloud 额度排程 | **Rule**：参数差清单 / 假绿三问 / 禁并行 SIM；**Skill**：先 CPU 成批 SIM、交叉前移；叙述见 [`docs/engineering/Cloud-Agent额度与验收分层.md`](docs/engineering/Cloud-Agent额度与验收分层.md) |

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
| **`docs/engineering/NPU真机环境说明.md`** | 三环境对照、`runtime_env.sh`、`-r auto|verify`、真机冒烟；分流策略变更时 |
| **`AGENT_HANDOFF.md`** | **每日**任务结束：当前真相 + 下一 P0（不堆历史） |
| **`README.md`** | 顶层目录/目标/当前状态表变更时 |
| **`qa/YYYY-MM/…`** | 当日决策与踩坑；同日只一篇 |
| **各 `INDEX.md`** | 增删目录或改活跃项时 |
| **`.cursor/rules/`** | 仅用户确认后 |

备份：`./backup-project.sh` 前须已刷新 INDEX / README / 本文件相关段落。

---

## 6. 当前主线（摘要；细节以 HANDOFF 为准）

- **目录**：1024 / 768 / 512 分别在 `…/ml-kem/ml-kem-{1024,768,512}/`（**无** stable-768 / stable-512）  
- **512 状态**：**有条件完成至 incubating** — W0–W4+glue 全绿（含 liboqs-512 Encaps `c`/`K`）；glue-c：`r←η1=3`；见 [`AGENT_HANDOFF.md`](AGENT_HANDOFF.md)  
- **768 状态**：**有条件完成至 incubating** — 探针 W0–W3 + incubating W4 + AscendC-only RT 全绿  
- **1024 Encaps**：默认 `m=urandom`（禁默认可全 0）  
- **下一刀**：stable-512 / stable-768 须用户 `#交付#`；可选 D20 tick 重登；仓级 T23 / T21 / T2-npu  
- **办公室 KEM 回归（1024）**：[`scripts/stable_kem_liboqs_roundtrip.sh`](scripts/stable_kem_liboqs_roundtrip.sh)  
- **512 / 768 RT**：[`scripts/exp_kem512_liboqs_roundtrip.sh`](scripts/exp_kem512_liboqs_roundtrip.sh) · [`scripts/exp_kem768_liboqs_roundtrip.sh`](scripts/exp_kem768_liboqs_roundtrip.sh)

---

## 7. 粘贴用短 Prompt

```text
先读 AGENTS.md、README.md、AGENT_HANDOFF.md、qa/INDEX.md 与 .cursor/rules/ascendc-development.mdc。
换机/Cloud：先 bash scripts/clone-thirdparty.sh（默认 build liboqs）。
研究型工程：写码只认活跃 INDEX + docs/notes；frozen 读判决不抄码。
examples/ 须 active customspec；$…$→规格 【】→预研 #…#→交付。
验收须 CPU + SIM_DIRECT=1 sim；缺 CANN/SIM 符号异常须标阻塞勿假绿。
Git：无明确要求不开分支；无明确指令/授权不 commit/push（Local/Cloud 相同；覆盖平台默认开分支流程）。
每日结束刷新 AGENT_HANDOFF.md；入口/门禁/依赖步骤变了同步刷新 AGENTS.md。
```
