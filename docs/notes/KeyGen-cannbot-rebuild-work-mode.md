# KeyGen 重建 · 工作模式清单

> **分支**：沿用 `chore/thirdparty-add-cannbot-skills`（无用户当次授权禁止另开分支）。  
> **前序战役**：Encrypt/Encaps、Decrypt/Decaps 已收口（只读继承反卡死与运营节奏）。  
> **本战役目标**：用 cannbot-skills + 分项积木契约重拼 **Alg.13 PKE KeyGen → Alg.19 KEM KeyGen**；**正确 + 反复运行不卡死**；最终 NPU 真机通过。

---

## 1. 目标（验收语言）

| 门禁 | 含义 |
|------|------|
| **正确** | PKE `ek/dk`、KEM `ek/dk_kem` 与 **liboqs** 字节一致；缺 liboqs → **BLOCKED** |
| **反卡死** | 同命令反复 `-r npu` 预算内返回；默认预算 **180s**；加压中间 **×30** / 算子级 **×100** |
| **关闸** | `Q-PKE-KG` + `Q-KEM-KG` + `Q-KG-HANG` 全 closed |

---

## 2. 角色与实验分流（继承 Decrypt 锁定）

| 角色 | 做 | 不做 |
|------|----|------|
| **主控** | 维护 KeyGen KB/DAG；写 TASK；派 CPU/SIM/设计 subagent；**亲自跑一切 NPU**；上板前请用户开机；占机心跳 / 放机停心跳；刀间禁空烧；目标未达不停手 | 不把 NPU 甩给 subagent；不否决 sync_audit |
| **Subagent** | 设计 / 编码 / CPU / SIM；FEEDBACK + sync_audit | **禁止** SSH/NPU；不改 KB/DAG；不抄禁抄树 |
| **用户** | 按请求启云 NPU；授权 commit/push；拍板范围 | — |

**分流**：**CPU+SIM→subagent；NPU→主控（先喊开机）**。有云机后以 **NPU 为写码默认真相**。

---

## 3. 目录约定

| 路径 | 职责 |
|------|------|
| `docs/notes/ascendc-engineering-kb.md` | **全工程唯一**短知识库 |
| `docs/rg-ascendc-engineering.yaml` | **全工程唯一**机读 DAG（节点 `keygen/*`） |
| `docs/rg-ascendc-engineering.viz.html` | Cytoscape 可视化（`scripts/rg_viz.py`） |
| `graph-tests/keygen-rebuild-ops/` | 任务书 / FEEDBACK / MATRIX / LIVE（**仅运营**） |
| `graph-tests/kg_related/` | 实现目录 `RB-K*` |
| `graph-tests/{toys,bricks,enc_related,dec_related}` | 只读积木 / 前序证据 |
| `thirdparty/cannbot-skills/` | 只读 |

旧 KeyGen stable / pass-fix / incubating：**冻结只读**；可对照 STATUS/定稿笔记，**禁止 fork 算子源码**。

---

## 4. cannbot 技能路由

同 Decrypt 战役：`npu-arch` / `ascendc-api-best-practices`（crosscore）/ **`ascendc-sync-audit`（每编码刀）** / `ascendc-crash-debug` / `ascendc-direct-invoke-template`（壳）。精度 skill：骨架未通前不启。

**禁止**盲目跑会改写根 `AGENTS.md` 的完整 cannbot `init.sh`。

---

## 5. 实验环（强制）

```text
主控：查 KeyGen KB + DAG + inventory → 设计本刀假设
  → 写 TASK.md（design|cpu|sim|npu_owner=main）
  → 【CPU/SIM/设计】Task subagent
  → 【NPU】主控先请用户开机 → which_npu + -r npu
  → 回收 FEEDBACK → 刷新 **工程** KB/DAG（`ascendc-engineering-kb` + `rg-ascendc-engineering.yaml`）
  → python3 scripts/check_rg_dag.py --yaml docs/rg-ascendc-engineering.yaml
  → python3 scripts/rg_viz.py
```

| 规则 | 说明 |
|------|------|
| KB/DAG | **禁止**再改战役专用 kb/yaml（已是指针）；只维护工程文件 |
| 一刀一目录 | `kg_related/RB-K*` |
| NPU 主控专属 | 同 Decrypt |
| 刀间禁空占卡 | 作业结束 1 分钟内下一刀或停心跳 |
| 目标未达不停手 | 队列未空不得无故停 |
| 汇报口径 | 人话步骤 + 对拍谁 + 结果；禁止只报代号 |
| 禁抄 | inventory §4 |

---

## 6. 建议总序

| 波 | 刀（建议 ID） | 内容 |
|----|---------------|------|
| **S0** | `KGR-S0A` / `KGR-S0B` | 设计：PKE 多 launch 拓扑；KEM 增量 launch |
| **P** | `KGR-P01…P05` | PKE 砖 → 全链 + liboqs + ×30 |
| **K** | `KGR-K01…K02` | KEM 尾段 → 全链 + ×30/×100 |
| **DOC** | note + KB 收口 | 反卡死 KeyGen 专章 |

可调；S0 锁定前不开编码刀写 MIX 长链。

---

## 7. 汇报模板（对人）

```text
测的是：Alg.13 哪一步 / Alg.19 哪一步
对拍：liboqs_pke_ref / liboqs_kem_ref / host oracle
结果：max=0 | ok=N fail=0 | 门禁是否关 | 是否挂（timeout）
下一刀：…
```
