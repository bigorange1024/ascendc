# graph-tests — 卡死排查 · 图谱驱动试验场

> **Encrypt 卡死重写（2026-09-06 起主线）**：[`toys/`](toys/INDEX.md) · [`enc_related/`](enc_related/INDEX.md) · KB [`docs/notes/Encrypt-hang-rewrite-kb.md`](../docs/notes/Encrypt-hang-rewrite-kb.md) · 新图 [`docs/rg-encrypt-hang-rewrite.yaml`](../docs/rg-encrypt-hang-rewrite.yaml)  
> **旧 Encrypt/l18 图（只读参考）**：[`docs/rg-encrypt-l18.yaml`](../docs/rg-encrypt-l18.yaml) + 历史 GT-*  
> **Decrypt fused**：**另立** [`decrypt/INDEX.md`](decrypt/INDEX.md) + [`docs/rg-decrypt-fused.yaml`](../docs/rg-decrypt-fused.yaml)  
> **最后刷新**：2026-09-06

Decrypt 真理：[`docs/rg-decrypt-fused.yaml`](../docs/rg-decrypt-fused.yaml)  
DAG 检查：`python3 scripts/check_rg_dag.py --yaml docs/rg-encrypt-hang-rewrite.yaml`  
（`thirdparty/reasoning-graph-skill` 若缺失则用上述脚本）

**知识来源（强制）**：不限当前检出分支。须主动 `git fetch` 并阅读**其它分支**上刚做的实验笔记（尤其 `origin/cursor/kem-2launch-sticky-1534` 的 `qa/2026-09/*`、HANDOFF），按准入规则沉淀进图谱；evidence 写明分支/路径。当前工作区没有的纪要 ≠ 可以当没发生。

---

## 1. 目标分层（勿混）

| 层级 | 目标 | 验收 |
|------|------|------|
| **最终（用户）** | NPU 实机上 Encrypt/`l18_l19` **不再 SynchronizeStream 卡死**，且 **正确性通过** | 图上 `Q-ULT` |
| **当前近目标** | 轻量 **toy** 模仿 Encrypt **任务链骨架**（桩哈希 + NTT / INTT / 内积 / encoding 串） | 图上 `D-NEAR` |
| **toy 验收** | **不对算法正确性**；**SIM 能跑完且快** | `Q-TOY-NTT` / `Q-TOY-GATE-INTT` |
| **本目录** | 验证**图谱推理出的单点假设**（比 toy 更碎的功能点） | 各子用例 + 反馈回写图谱 |

**成功标准（总闸，勿忘）**

- toy / `graph-tests` / 图谱推理 **不是终点**：实验链必须能**收敛到解决 Encrypt 运行卡死**（最终在 NPU 上验证）。  
- 用户可协助把**充分实验后的** toy/补丁拿到实机跑；**实机拷贝机会稀缺**——主控须先在 **SIM 上把假设证够、失败教训沉够、复现步骤写清**，再打包请用户上机。  
- **禁止**「SIM 随便绿一下就催上机」；也禁止无限玩具化、偏离卡死根因。

**验证口径（强制）**：过程以 **SIM 为关键**；最终盯 **NPU**。  
**CPU 测试及 CPU 经验：无参考价值，不进图谱、不作推理依据**（`D-RG-SIM-PRIMARY`）。

Decrypt 卡死：**独立图谱与试验场**（用户 2026-09-03 订正，不再等 Encrypt 走通）：[`decrypt/INDEX.md`](decrypt/INDEX.md) · [`docs/rg-decrypt-fused.yaml`](../docs/rg-decrypt-fused.yaml)。

### 1.1 请用户上机前的清单（主控自检）

未勾满 **不得** 请用户拷代码上实机：

1. 相关假设在图谱上有节点；SIM 证据已写入 evidence / STATUS  
2. 失败对照已做过或已标 `fail-lessons`（不是只报一路绿）  
3. 上机步骤 ≤ 半页：目录、编译/FORCE、env、期望日志（挂点/TRACE）、带回什么  
4. 改动集尽量小、可单独同步；说明与 sticky/main 的差异  
5. 若上机失败：用户带回日志后主控先刷新图谱，再设计下一刀——不连环催拷

---

## 2. 图谱：必须做的事

机读文件：`docs/rg-encrypt-l18.yaml`（唯一真理源；HTML 只是渲染）。

### 2.0 来源范围（跨分支）

| 来源 | 怎么用 |
|------|--------|
| 当前分支 `qa/` / STATUS / 代码 | 照常提炼 |
| **其它远程分支** 的 qa、HANDOFF、失败实验记录 | **必须**纳入候选；`git show origin/<branch>:path` 阅读后沉淀 |
| `docs/notes`、API 查阅索引 | 仅当直接服务本线 debug |
| sticky 分支已证伪/失败 decision | 优先沉 `fail-lessons` / `retracted`，防本分支重踩 |

刷新图谱前自检：是否漏了 sticky/其它 agent 分支上同主题最新纪要？

### 2.1 谁改图谱

| 角色 | 图谱 |
|------|------|
| **主控 Agent（我）** | **唯一**负责：加节点、改 status、证伪整链推翻、写 changelog、重渲 HTML |
| **Subagent** | **只读**参考；**禁止**改 yaml |

### 2.2 沉淀什么 / 不沉淀什么

**必须沉淀（服务 debug）**

- 与卡死/FSM/CrossCore/GATE/TRACE/粘性直接相关的 fact / inference / decision / question  
- **失败实验与失败经验**（一等公民）：`inactive` decision + `fail-lessons` 教训；禁止只堆「测绿」

**禁止进图**

- 方法论汇报、组织事务、无关目录治理散文  
- **CPU 路径/经验**当作证据或推荐  
- 「为绿而绿」且严重伤性能的推荐知识：逐步 GM 外搬、堆 Host launch、放弃融合换正确性等（`D-RG-NO-SLOW-CORRECTNESS`）  
- 把 CPU 多 launch 孪生写成生产/消粘方案  

治理节点：`D-RG-DEBUG-ONLY` · `D-RG-FAIL-MUST-KEEP` · `D-RG-NO-SLOW-CORRECTNESS` · `D-RG-SIM-PRIMARY`

### 2.3 工作环（每次假设）

```text
查图 (rg_query) → 提出/挂靠新假设 (unverified|conditional)
  → 设计实验（本目录或 ascendc-tests toy）
  → 下发 subagent（任务模板 + 必读节点 + 禁止项）
  → 收反馈（仅 SIM / NPU 证据）
  → 刷新图谱：成立则沉淀；失败则 inactive/retracted + J-FAIL-*；整链反向审查
  → rg_validate → 重渲 HTML
```

证伪时：**不能**只在新节点正文里顺嘴提旧结论错了；必须改旧节点 status。

### 2.4 已收入的失败教训（勿再试）

| 节点 | 教训 |
|------|------|
| `J-FAIL-INTT-FLAGS-57` | INTT 改 flag 5/7 → SIM 超时 |
| `J-FAIL-SYNCALL-SOFTSYNC` | AIC Wait 中 SyncAll / 自造 SoftSync → SIM 挂 |
| `J-FAIL-SPLIT-AS-SUFFICIENT` | 拆双 Cube / 改 2–3 launch ≠ 消粘充分条件 |
| `J-FAIL-DEL-OUT` | 删 out 不可靠；靠单卡 reset |
| `J-FAIL-ONLY-L18-STORY` | 勿把根因收成「只卡第二段 l18」 |

另：`D-NO-SYNCALL-WHILE-AIC-WAIT`、`D-FOLLOW-DECRYPT-SOFTSYNC` 为 **active 禁令**。

### 2.5 常用命令

```bash
# 结构校验
python3 thirdparty/reasoning-graph-skill/scripts/rg_validate.py --yaml docs/rg-encrypt-l18.yaml

# 检索
python3 thirdparty/reasoning-graph-skill/scripts/rg_query.py --yaml docs/rg-encrypt-l18.yaml --search 'TRACE'
python3 thirdparty/reasoning-graph-skill/scripts/rg_query.py --yaml docs/rg-encrypt-l18.yaml --status open

# 重渲（改 yaml 后）
python3 thirdparty/reasoning-graph-skill/scripts/rg_render.py \
  --yaml docs/rg-encrypt-l18.yaml \
  --out /opt/cursor/artifacts/rg-encrypt-l18.html
# 若 8765 服务于 methodology-demo：再 cp 一份到该目录同名 html
```

Skill 本体留在 `thirdparty/reasoning-graph-skill/`，**暂不**整包迁入 `.cursor/skills/`。

---

## 3. 代码试验场怎么拆

| 位置 | 用途 |
|------|------|
| **`ascendc-tests/<toy>/`** | Encrypt **任务链骨架**探针（快 SIM；不对算法正确性） |
| **`graph-tests/<case>/`** | 图谱推出的**更碎**假设（单握手、单 GATE、单 TRACE 行为等） |
| **stable Encrypt** | 本阶段**禁止**当主调试场（全量 SHAKE SIM 太慢；`D-NO-FULL-ENCRYPT-ITER`） |

本目录子用例约定：

- 每案一个子目录：`graph-tests/<短名>/`  
- 必有：`STATUS.md`（假设节点 id、SIM 命令、结果、是否回写图谱）  
- 验收：**SIM**（`SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` 或等价）；**不把 CPU 当证据**  
- 禁并行多路 SIM；禁从 `frozen/` 抄码  

---

## 4. 主控 vs Subagent

| 主控 | Subagent |
|------|----------|
| 定目标、管图谱、设计实验、分析反馈、决定下一刀 | 按任务模板写码 / 跑 SIM / 填反馈 |
| 下发前附：图谱路径、必读节点、禁止项（inactive/retracted） | 可读图谱，**不改** yaml |
| 收反馈后刷新图谱 | 不扩 scope |

### 4.1 下发任务模板（主控填写）

完整模板见 **§4.4**（必须含 `WALL_CLOCK_BUDGET` / `MAX_SIM_ATTEMPTS` / `MAX_REWORK_ROUNDS` / `DONE_DEFINITION` / `ABORT_IF`；禁止无预算下发）。

### 4.2 反馈模板（Subagent 填写）

```text
【任务 id】
【SIM 命令】
【结果】PASS / FAIL / HANG / TIMEOUT / BLOCKED / ABORT
【挂点】最后日志行；有无 TRACE 槽
【产物路径】log / STATUS
【是否改了图谱】否（应始终为否）
【是否 push】否（应始终为否）
【建议主控关注】一句话
```

### 4.3 Subagent 硬规则（时间 / 并发 / Git）— 主控必须执行

**Git（总闸）**

- **禁止**主控或 subagent **自主** `git push` / 开 PR / 擅自开分支（无用户当次明确指令）。  
- 改完先汇报；等用户说「提交/推送」再动远程。

**并发**

- **同一时间只派 1 个**干活 subagent（尤其涉及 `run.sh -r sim`）。  
- **禁止**并行多路 SIM；上一刀未收束（反馈已交或已 ABORT）不得派下一刀。

**时限与止损（防傻等 / 死循环 / 无限返工）**

下发任务时主控**必须**写明下列字段；缺一不可：

| 字段 | 默认建议 | 含义 |
|------|----------|------|
| `WALL_CLOCK_BUDGET` | 实现 25–40 min；单次 SIM 跟用例 `KERNEL_COMPUTE_BUDGET_SEC` | 墙钟上限；到点未交反馈 → 主控 **ABORT** |
| `MAX_SIM_ATTEMPTS` | **2** | 同一任务最多 SIM 轮次（含修完重跑） |
| `MAX_REWORK_ROUNDS` | **1** | 主控根据反馈让其返工的最多次数；满则主控收束改规格/换假设，**不**让同一 subagent 无限改 |
| `DONE_DEFINITION` | 一条可判定句 | 例：「SIM 进程退出且无 hang；交反馈模板」——不对算法正确性时可写「跑完即可」 |
| `ABORT_IF` | 清单 | 例：编译连续失败 2 次；SIM 同挂点 2 次；超预算；擅自改图谱/push |

**Subagent 行为约定（写入每份任务正文）**

1. 只做本任务范围；卡住 **15 分钟无进展** → 立即按反馈模板报 `BLOCKED`（写清卡点），**不要**默默死磕。  
2. SIM hang/timeout → 记最后日志行后停；**禁止**无新假设地同一命令连刷。  
3. **禁止**自行扩大 scope、改 `docs/rg-*.yaml`、`git push`、并行再开 SIM。  
4. 到 `WALL_CLOCK_BUDGET` 前至少留下：当前 diff 说明 + 反馈模板（即使 FAIL）。  

**主控行为约定**

1. 不空等：预算到点或反馈 `BLOCKED`/`ABORT` → **立刻**收束；改图谱或重写更小任务后再派（仍同时仅 1 个）。  
2. 同一失败挂点已出现在图谱 `fail-lessons` / inactive 的，**不得**再派 subagent 重试同方案。  
3. 返工满 `MAX_REWORK_ROUNDS` → 主控自己缩小假设或换实验，而不是继续催同一 agent「再改改」。

### 4.4 下发任务模板（主控填写，含时限字段）

```text
【任务 id】GT-YYYYMMDD-N
【目标】一句话
【图谱】docs/rg-encrypt-l18.yaml
【必读节点】id 列表 + 各一句话
【禁止】勿重试 inactive/retracted；勿 CPU 验收；勿 SyncAll@AIC-Wait；勿 frozen 抄码；勿改图谱；勿 git push
【要做】目录路径、实现范围、SIM 命令
【不要做】范围外改动
【WALL_CLOCK_BUDGET】… 分钟
【MAX_SIM_ATTEMPTS】2
【MAX_REWORK_ROUNDS】1
【DONE_DEFINITION】…
【ABORT_IF】…
【反馈】填 §4.2；到点也必须交
```

---

## 5. 当前开放问题（从图抄来，改图后同步这里）

| id | 问题 |
|----|------|
| `Q-ULT` | NPU 上 l18 是否不卡且正确？ |
| `Q-EMPTY-TRACE` | 空 TRACE 时卡在哪次 Wait，AIV0 为何零 mark？ |
| `Q-TOY-SIM-MULTI-LAUNCH-HANG` | skel1 同进程多 launch SIM 是否挂？ |
| `Q-KEYGEN-CONTRAST` | 同卡多轮 KeyGen 是否不粘？（对照；NPU） |

未证主线：`J-PRIMARY-FSMWAIT` · `J-COMMON-FACTOR` · `J-EMPTY-TRACE-*` · `J-TOY-ISOLATES-FSM`(conditional)

---

## 6. 建议下一刀顺序（2026-09-03 再刷新）

用户订正：**暂时不能上机（明天）**；toy 目的含 **SIM 多跑加压**，看会不会卡死。上机清单仍保留，不催拷。

1. **GT-5..7 已绿**：同核多 launch + 两段 2-launch 在 SIM **均未挂**  
2. **下一刀待定**：继续逼近生产 Encrypt 骨架，或明日上机 `NPU-TRIP-20260903-skel1.md`  
3. 勿把 toy 未挂当成 Encrypt 消粘完成（`Q-ULT` 仍 open） 

toy 绿了却解释不了 Encrypt 卡死 → 主控须收束假设，**禁止**当成最终成功。  

---

## 7. 目录内容

| 路径 | 说明 |
|------|------|
| `INDEX.md` | 本文件（任务总说明；随目标/图谱治理刷新） |
| `GT-20260903-1.md` | 第一刀：NTT flag 1/3 SIM — **PASS** |
| `GT-20260903-2.md` | 第二刀：GATE+INTT（跳过同核 NTT）— **PASS** |
| `GT-20260903-3.md` | 第三刀：同核 NTT→GATE→INTT + TRACE — **PASS**（AIV0 可见；AIC 标量假空） |
| `GT-20260903-4.md` | 第四刀：TRACE DataCopy — **PASS**（AIV0+AIC 可见） |
| `GT-20260903-5.md` | 第五刀：同进程 16×launch — **PASS**（未挂） |
| `GT-20260903-6.md` | 第六刀：32× + 3 进程 — **PASS**（未挂） |
| `GT-20260903-7.md` | 第七刀：两段 Host launch — **PASS**（未挂） |
| `NPU-TRIP-20260903-skel1.md` | 偶发上机清单（skel1；推迟到有卡） |
| [`decrypt/`](decrypt/INDEX.md) | **Decrypt fused** 独立试验场（SoftSync toy 等） |
| [`NPU-TRIP-20260904-decrypt.md`](decrypt/NPU-TRIP-20260904-decrypt.md) | 明日上机：Decrypt toy + stable TRACE |
| `<case>/` | 后续单点试验 |

增删子目录时同步本 INDEX。顶层结构变更已记入根 `README.md`。
