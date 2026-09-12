# Encrypt + Encaps × cann-ntt · SIM-only 全套实验计划

> **锁定日期**：2026-09-11  
> **分支**：现有 sticky（禁止擅自开分支）  
> **参数**：ML-KEM-**1024**（\(k=4,n=256,q=3329,\eta_1=\eta_2=2,d_u=11,d_v=5\)）  
> **积木**：迁入 **cann-ntt** MIX NTT/INTT（Cube+Vector；非纯 Vector）  
> **主目标**：绝对不卡死（继承）+ **本阶段补齐 SIM 正确性与 Encaps 形**  
> **运行门禁**：**仅 CPU + `SIM_DIRECT=1` sim**；**NPU = 封锁**（见 §0）

---

## 0. SIM-only / 禁空转（用户硬约束）

| 规则 | 说明 |
|------|------|
| **R0** | 本战役默认 **不上 NPU**；QUEUE 中带 `NPU` 标签的刀 **status=BLOCKED_UNTIL_USER** |
| **R1** | 禁止启动板端 keepalive / 心跳占卡；用户休息期间 **零 SSH 作业** |
| **R2** | 一刀一目录；同刻 **禁止并行多路 SIM**（Rule） |
| **R3** | 刀失败：写 FEEDBACK → 图谱挂失败节点 → **停**；禁止在红基础上叠刀 |
| **R4** | SIM 墙钟预算写进 TASK；超时 **中止回报**，不挂后台空转 |
| **R5** | 用户明确说「可以上机」之前，主控 **不得** 把 WN 刀改成可执行 |

---

## 1. 已锁定事实（继承，勿重采）

| ID | 事实 | 证据 |
|----|------|------|
| F1 | Host 多段 + cann-ntt 短 MIX NTT/INTT；Prep/Matvec/Pack 偏 AIV | KB A1–A3；能力清单 |
| F2 | EN01 NTT / EN02 NTT+INTT SIM 不挂；KEM q=3329 | `enc_cann_ntt/EN01|EN02` |
| F3 | EN07 贯通链、EN09 设备 SampleNTT、EN08/12 sticky SIM 不挂 | INDEX EN07–12 |
| F4 | NTT I/O 以 cann-ntt 布局为准；旧 Tag5T 布局不默认兼容 | KB A4；本轮 SIM 对比 |
| F5 | 图谱开放问：`Q-CORRECTNESS-FULL`（权威逐字节）仍 open | `rg-encrypt-cann-ntt.yaml` |
| F6 | Encaps（Alg.16/20）**尚未**在本线独立成战役 | 能力清单仅 Alg.14 |

**推论**：不挂骨架已有 → 本战役 SIM 主攻 **正确性闭环** 与 **Encaps 编排**；不是再证「cann-ntt 会不会挂」。

---

## 2. 目标与非目标

### 2.1 目标

| 级别 | 指标 | 通过线 |
|------|------|--------|
| **不挂（继承）** | 每刀 CPU + SIM_DIRECT 正常退出；无 stray dump | 必须 |
| **同步** | 有设备同步则 `sync_audit` **红线=0**（禁否决） | 必须 |
| **Encrypt 正确性（W1）** | 同种子下 `c` 与 liboqs PKE Encrypt **逐字节**（或登记允许的域） | W1 出口必须 |
| **Encaps 形（W2–W3）** | Host：\(m\)→G→\((K,r)\)→Encrypt→\(c\)；输出 `c`/`K`；SIM sticky 多轮不挂 | W3 出口必须 |
| **Encaps 交叉（W3 可选加强）** | `c`/`K` vs liboqs Encaps | 有条件成功；非第一刀阻塞 |

### 2.2 非目标（本阶段）

- NPU 性能 / msopprof / 锁频。  
- 再压 Host launch 数「好看」。  
- 把 Tag5T 核抄进本线当 NTT。  
- Decaps / 全 KEM roundtrip stable 晋级（另战役）。  
- 休息时段占板。

---

## 3. 架构不变量（Encrypt / Encaps 共用）

```text
[Host]
  Encaps 头：m ← {0,1}^256； (K̄,r) ← G(m‖H(ek))   ← 可先 Host，再迁设备 SHA3
  Encrypt 段：
    L1 Prep   : AIV_ONLY（SampleNTT Â / CBD y,e1,e2 / decode t）
    L2 NTT(y) : 短 MIX cann-ntt（独立 launch）
    L3 Matvec : AIV_ONLY（Âᵀ∘ŷ + …）  ← 布局 = cann-ntt 输出
    L4 INTT   : 短 MIX cann-ntt 逆矩阵（独立 launch）
    L5 Pack   : AIV_ONLY（Compress + ByteEncode → c）
  Encaps 尾：K ← K̄（隐式拒绝留给 Decaps 战役）
```

| 不变量 | 含义 |
|--------|------|
| A1 | NTT/INTT **独立短 MIX launch**；禁与 Matvec 融成单胖 MIX |
| A2 | 禁自研 GATE 4/8、禁 CrossCore flag 5/7、禁 Wait 环 SyncAll |
| A3 | Matvec **重接** cann-ntt 布局；禁止假设 Tag5T S0/mat_c |
| A4 | 一刀一目录；坏了回退 EN07/EN09/EN12 等干净节点 |
| A5 | 正确性未绿前，**性能刀不做** |

---

## 4. 推理图谱用法（强制）

本战役图：[`docs/rg-enc-encaps-cann-ntt-sim.yaml`](../../docs/rg-enc-encaps-cann-ntt-sim.yaml)  
校验：`python3 thirdparty/reasoning-graph-skill/scripts/rg_validate.py --yaml docs/rg-enc-encaps-cann-ntt-sim.yaml`  
渲染：`rg_render.py --yaml … --out /opt/cursor/artifacts/rg-enc-encaps-cann-ntt-sim.html`

| 动作 | 图谱 |
|------|------|
| 开刀前 | 读 open `Q-*`；本刀挂 `D-EXP-ENxx` / `D-EXP-EPxx` |
| 刀后绿 | 加 `F-*` evidence；若回答某 Q 则 `answered_by` |
| 刀后红 | `F-FAIL-*` + 停；更新 deps 冲击审查 |
| 与旧 Encrypt 图关系 | **继承** `rg-encrypt-cann-ntt` 的不挂结论；**不复制** NPU 节点为本阶段可执行 |

---

## 5. cannbot-skills 用法（每刀）

根：`thirdparty/cannbot-skills/`（不 vendor 进 `.cursor/skills`）。

| 阶段 | 主控设计必载 | Subagent 执行 |
|------|--------------|---------------|
| 开刀 | `ops/ascendc-tiling-design`、`ops/ascendc-api-best-practices`（crosscore）、`ops/npu-arch`（按需只读） | — |
| 壳 | `ops/ascendc-direct-invoke-template`（对照；优先复制本仓 `enc_cann_ntt` 壳） | 按 TASK 建目录 |
| 同步 | `ops/ascendc-sync-audit` | `sync_audit.py`；红线不可否决 |
| 卡死（SIM 挂死/超时） | sync-audit → deadlock-triage；`ascendc-crash-debug` / `runtime-debug` | 贴结论到 FEEDBACK |
| 正确性（W1+） | `ops/ascendc-precision-debug`、`ops/ops-precision-standard`（设计对照） | golden / liboqs 交叉 |
| 环境 | `ops/ascendc-env-check` | Cloud 无卡则 SIM；缺 CANN → BLOCKED |
| **禁止本阶段** | `ops/ops-profiling` 真机采集 | 勿上板 |

过程目录（可选）：`.cannbot/mlkem-enc-encaps-cann-ntt-sim/`。

---

## 6. 波次与刀单（SIM 顺序）

### W0 — 锁盘与基线复验（主控+轻量 SIM）

| 刀 | 目录/动作 | 目标 | cannbot | 预算 |
|----|-----------|------|---------|------|
| **EE-W0a** | 本目录 PLAN/QUEUE/INDEX + `rg-*.yaml` | 文档齐；`rg_validate` OK | — | 文档 |
| **EE-W0b** | 复跑 `enc_cann_ntt/EN09` 或 EN07：`cpu` + `SIM_DIRECT=1 sim` | 确认基线可复现不挂 | sync_audit 若改码 | SIM ≤ 既有 STATUS |

### W1 — Encrypt 权威正确性（SIM）

> 在 **不改 NTT 积木** 前提下，把贯通链接到 **liboqs 字节 oracle**。

| 刀 | 建议目录 | 目标 | 允许读 | 禁止 | 验收 |
|----|----------|------|--------|------|------|
| **EN13** | `enc_cann_ntt/EN13-encrypt-liboqs-cross/` | Host 编排 + cann-ntt；`c` vs liboqs Encrypt 同 \(ek,m,r\) | EN07/09 契约、shared codec、liboqs glue | 抄 stable encrypt 核 | CPU+SIM；max=0；sync 红线0 |
| **EN14** | `…/EN14-encrypt-cross-sticky/` | 同进程 R≥8 SIM sticky；每轮换种子仍 max=0 | EN13 | 上 NPU | SIM sticky；无 hang |

**失败三问**（假绿）：golden 是否与实现同源？权威交叉是否已跑？跨核 Sync 是否漏？

### W2 — Encaps 积木（SIM，先壳后真）

| 刀 | 建议目录 | 目标 | 能力 ID | 验收 |
|----|----------|------|---------|------|
| **EP01** | `enc_cann_ntt/EP01-encaps-host-skel/` | Host：读 ek；采样 m；算 H(ek)、G；拆 \(K,r\)；**桩** Encrypt 输出 | H3/H4 Host | CPU+SIM 不挂；结构长度对 |
| **EP02** | `…/EP02-encaps-call-encrypt/` | 真调 EN13（或 EN09 链）Encrypt；写出 `c`/`K` | 复用 Encrypt | CPU+SIM；长度+自洽 |
| **EP03** | `…/EP03-encaps-device-hash/` | 设备 SHA3：H(ek)/G（可选；可后置） | H1–H3 | CPU+SIM；vs Host hash max=0 |

### W3 — Encaps 贯通 + sticky（SIM）

| 刀 | 建议目录 | 目标 | 验收 |
|----|----------|------|------|
| **EP04** | `…/EP04-encaps-liboqs-cross/` | `c`/`K` vs liboqs Encaps（同 m 或 derand 约定） | CPU+SIM；交叉门禁写清 |
| **EP05** | `…/EP05-encaps-sticky-sim/` | 同 session R≥16 SIM；不挂 + 抽样交叉 | SIM；禁 NPU |

### WN — NPU（默认封锁）

| 刀 | 触发 | 内容 |
|----|------|------|
| **EN15 / EP10** | 用户明确授权上机 | 单轮不挂 → 多轮 → sticky；**仍**禁休息空转 |

---

## 7. 单刀生命周期（与 workmode 对齐）

```text
① 主控：能力清单 + KB + 本战役 QUEUE/图谱（失败优先）
② 主控：加载 §5 cannbot skills → 写 ENxx/EPxx-TASK.md
③ 派 subagent（只 SIM；TASK 写明禁 -r npu）
④ 编码 → cpu → SIM_DIRECT=1 sim → sync_audit
⑤ FEEDBACK：PASS/FAIL/BLOCKED + 命令 + 红线 + 一句教训
⑥ 主控：回写 KB 短条 + DAG 节点 + QUEUE
⑦ 下一刀仅当本刀绿；否则停
```

任务书最低字段：同 `Encrypt-cann-ntt-workmode.md` §4，并追加：

- **运行态**：`SIM_ONLY=1`；禁止 `-r npu`  
- **超时**：SIM 墙钟上限（建议单刀 ≤30–90 min，按链长度写死）  
- **失败动作**：杀进程、写 FEEDBACK、**不**启 keepalive  

---

## 8. 成功 / 停损

| 出口 | 条件 |
|------|------|
| **SIM 战役有条件完成** | EN13 交叉绿 + EP02 形绿 + EP05 sticky SIM 不挂 |
| **SIM 战役强完成** | 上述 + EP04 liboqs Encaps 交叉绿 |
| **停损** | 任刀 SIM hang/红线/交叉红且假绿三问未答清 → 停并请用户拍板 |
| **转入 WN** | 仅用户授权；否则 QUEUE 保持 BLOCKED |

---

## 9. 与「你的 Tag5T」关系（本战役）

- **本线 NTT = cann-ntt**（已锁定 Encrypt 重建积木）。  
- Tag5T 仅作 **对照/契约阅读**；禁止抄核进 EN/EP 树。  
- 若日后要 KeyGen 换积木，另开战役；不与本 QUEUE 混刀。
