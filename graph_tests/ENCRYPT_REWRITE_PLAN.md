# Encrypt 重写计划（本分支 · 与 PR#19 不同路）

**分支**：`cursor/kem-2launch-sticky-1534`  
**最终目标**：Encrypt **实机无卡死且正确**（自底向上重拼，禁抄现 Encrypt）。

---

## 0. 本分支工作方式（用户锁 · 2026-09-07）

与其它分支（如 `cann-ntt-operator-refactor` 的 T01–T07 / N0–N10 套件）**分开**；本分支主叙事是图谱 + 真积木 toys + NPU 挂因阶梯。

**昇腾工程能力层 = 充分使用 `thirdparty/cannbot-skills`（就地读用，不整仓拷进 `.cursor/skills/`，不做 vendor 草案）。**

| 层 | 来源 | 管什么 |
|----|------|--------|
| **领域 / 挂因 / 路线** | 本仓知识库 + `rg-encrypt-npu-hangfree` + `BRANCHING` | FIPS 203 形态、假说、下一刀、上机反馈 |
| **AscendC 怎么写对 / 怎么查同步卡死** | **cannbot-skills** | 同步审计、卡死调试、API、tiling、环境、仿真、profiling |
| **本仓平台经验** | `ascendc-engineering-notes` | KernelLaunch / CPU·SIM 差异 / 会话排程 |

**写/改任何 Encrypt 相关 AscendC 前**：按阶段打开下表对应 cannbot Skill（读 `SKILL.md` + 必要时跑其 `scripts/`），再编码；禁止只凭本仓记忆写 CrossCore / Pipe。

### 0.1 阶段 ↔ cannbot Skill（强制对照）

| 阶段 | 做什么 | 必用 cannbot |
|------|--------|----------------|
| 环境 / 910B3 上机 | 设备、CANN、卡号 | `ascendc-env-check`、`cann-env-setup`、`npu-arch` |
| 核内流水 / API | DataCopy、Pipe、Cube/Vec | `ascendc-api-best-practices`（含 crosscore / pipeline） |
| MIX 握手 / GATE / SET | 防死等、flag 配对 | **`ascendc-sync-audit`**（优先 `deadlock-triage` / `full-audit` + `sync_audit.py`） |
| 实机/SIM 卡死 | hang vs 秒失败、plog | **`ascendc-crash-debug`** |
| 秒失败 / ACL 码 | 非 sticky hang | `ascendc-runtime-debug` |
| 形状 / UB / 分核 | tiling 与核设计 | `ascendc-tiling-design`、`ascendc-whitebox-design`（按需） |
| SIM 穷尽 | 仿真与流水 | `ops-simulator` |
| 正确性阶段（挂因之后） | 精度对照 | `ascendc-precision-debug`、`ops-precision-standard` |
| 性能阶段（更后） | profiling | `ops-profiling`、`ascendc-perf-optimize` |

**不用作本分支 Encrypt 主生成器**：Catlass / Blaze / PyPTO / TileLang / Triton / torch.compile 全流程（与 Tag5T 自研路径不同构；可只读参考 Cube 常识）。

路径根：`thirdparty/cannbot-skills/ops/<skill>/`。缺仓时：`ONLY=cannbot-skills BUILD_LIBOQS=0 bash scripts/clone-thirdparty.sh`。

---

## 1. 现状快照（2026-09-07）

- SIM 积木 E01–E15 + NPU_SUITE 包装 E16 ✅  
- NPU 单轮 C0/C1/C2 全绿（N7）→ 分支 B3  
- **知识库 + 图谱 v2 已从头刷新（cannbot 中心）**：`docs/notes/Encrypt-实机无卡死-知识库.md` + `docs/rg-encrypt-npu-hangfree.yaml`（`rg_validate` OK；`D-graph-and-cannbot` / `D-next-rxn-or-gap`）  
- cannbot 基线：toys e01/e13/e15 + 只读 l18 已跑 `sync_audit`（SYNC-03 同侧候选假阳性，禁自动改绿核）  
- 下一挂因刀曾拟 R×N(C2×7)；**用户暂停零散上机**；910B3 云主机已调通，**有时限，须用户明确授权再连**  
- 正确性 / ByteDecode：挂因未明前不做  
- **硬锁**：此后每刀实验/写码 = 遍历图谱 + 按 §0.1 打开 cannbot

---

## 2. 「从头做好 Encrypt」在本分支的含义

1. **不抄**现 stable/pass Encrypt；可读契约与失败。  
2. 积木自底向上（已有 toys）→ 用 cannbot **同步审计**把 MIX/GATE 写到可过静态红线 → 再 NPU 挂因阶梯。  
3. 挂因钉死后才进入正确性（仍用 cannbot precision 技能，不用对方分支那套 N0–N10 叙事）。  
4. 上机反馈：用户只打字；禁回传文件。

---

## 3. 上机入口（有额度且用户下令时）

- 单轮套件：`graph_tests/npu_suite/TOMORROW_NPU.md` / `run_all_npu.sh`  
- 多轮：`run_rxn_npu.sh` 或等价 `TOY_ROUNDS=7` 跑 C2  
- 回报：`REPORT:` / `SUMMARY` 打字即可  
