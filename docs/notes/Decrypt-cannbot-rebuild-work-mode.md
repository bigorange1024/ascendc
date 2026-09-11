# Decrypt / Decaps 重建 · 工作模式清单

> **分支**：沿用 `chore/thirdparty-add-cannbot-skills`（无用户当次授权禁止另开分支）。  
> **前序战役**：Encrypt/Encaps 已收口（`Encrypt-cannbot-rebuild-*` · T22–T24 · Q-ULT）。  
> **本战役目标**：用 cannbot-skills + 已有 NTT/编解码/Encaps **积木契约** 重拼 **Alg.15 Decrypt → Alg.21 Decaps**；**正确 + 反复运行不卡死**；最终 NPU 真机通过。

---

## 1. 目标（验收语言）

| 门禁 | 含义 |
|------|------|
| **正确** | Decrypt `m`、Decaps `K` 与 **liboqs** 字节一致；缺 liboqs → **BLOCKED**，禁 python 冒充权威 |
| **反卡死** | 同命令反复 `-r npu` 均在预算内返回；默认 `KERNEL_COMPUTE_BUDGET_SEC=180`；超时=缺陷。加压：**中间×30** / **算子级×100** |
| **关闸** | 设备 Encaps↔设备 Decaps 往返绿 + 压测（关 `Q-RT-HANG`） |

---

## 2. 角色与实验分流（2026-09-09 锁定）

| 角色 | 做 | 不做 |
|------|----|------|
| **主控（本会话）** | 读 cannbot；维护 KB/DAG；写 TASK；派 **CPU/SIM/设计** subagent；**亲自跑一切 NPU**；**上板前先请用户启动云 NPU**（默认未开，勿空连/假绿）；用户开机后若 SIM 慢可 **直接 NPU 同实验**；回收反馈；刷新 KB/DAG | 不把 NPU/SSH 甩给 subagent；不擅自假定云机已开；不否决 sync_audit 红线 |
| **Subagent** | 设计评审；在新目录编码；跑 **CPU** 与（任务书点名的）**SIM**；交 FEEDBACK + logs + sync_audit | **禁止** SSH / Tailscale / `-r npu` / 读云主机密钥；不定战役目标；**不改** KB/DAG；不抄禁抄树 |
| **用户** | **按主控请求启动云 NPU**；不必给每个 subagent 单独授云权；授权提交推送；拍板范围 | 不填复杂表 |

**分流一句话**：**CPU + SIM → subagent；NPU → 主控（先喊用户开机）。** 原因：subagent 连云需逐次授权；云机默认关机。

---

## 3. 目录约定

| 路径 | 职责 |
|------|------|
| `docs/notes/Decrypt-cannbot-rebuild-capability-inventory.md` | 积木与缺口 |
| `docs/notes/Decrypt-cannbot-rebuild-work-mode.md` | 本文件（角色/分流） |
| `docs/notes/ascendc-engineering-kb.md` | **全工程唯一**短知识库 |
| `docs/rg-ascendc-engineering.yaml` | **全工程唯一**机读 DAG（节点 `decrypt/*`） |
| `graph-tests/decrypt-rebuild-ops/` | 任务书 / FEEDBACK / logs（**仅运营**） |
| `graph-tests/dec_related/`（开刀后建） | Decrypt/Decaps 实现目录 `RB-D*` |
| `graph-tests/{toys,bricks,enc_related}` | 只读积木 / Encaps 收口证据（禁把 T25–T27 当模板） |
| `thirdparty/cannbot-skills/` | 只读；**不** vendor 进 `.cursor/skills`（除非用户确认） |
| `.cannbot/mlkem-pke-decrypt-rebuild/` | cannbot 过程件（按需） |

旧 `examples/**/decrypt|decaps`、`pass-fix-*-alg15|21*`、临时 `RB-T25/26/27` 源码：**冻结只读**；可对照 STATUS/FEEDBACK，**禁止 fork 源码**。

---

## 4. cannbot 技能路由（设计 / 编码）

| 阶段 | 主控必载 | 产出 |
|------|----------|------|
| 环境 / 架构 | `npu-arch`、`ascendc-env-check` | Soc=910B3/B4 → DAV_2201；单卡 `ASCEND_DEVICE_ID=0` |
| 需求 / 白盒 | `ascendc-docs-gen`、`ascendc-whitebox-design`、`ascendc-tiling-design` | 可选 `.cannbot/.../01-requirement.md` |
| CrossCore | `ascendc-api-best-practices` → `references/api-crosscore-sync.md` | 任务书 flag/拆 launch 约束 |
| 编码门禁 | **`ascendc-sync-audit`**（每编码刀必跑） | JSON → `decrypt-rebuild-ops/tasks/<ID>/logs/` |
| 卡死分诊 | `ascendc-crash-debug`、sync-audit `deadlock-triage` | 挂点假设 → 新 DAG `X*` |
| 工程壳 | `ascendc-direct-invoke-template`（壳参考）；本仓 `run.sh` 惯例 | 新目录脚手架 |
| 精度 | `ascendc-precision-debug` | **卡死未清 / 骨架未通前不启** |

**禁止**：盲目跑会改写根 `AGENTS.md` 的完整 cannbot `init.sh`。

---

## 5. 实验环（强制）

```text
主控：查 Decrypt KB + DAG + inventory → cannbot 设计本刀假设与验收
  → 写 TASK.md（标明本刀：design|cpu|sim|npu_owner=main）
  → 【CPU/SIM/设计】Task subagent（只读 KB/DAG；本机跑）
  → 【NPU】主控**先请用户启动云机** → which_npu + -r npu（可与慢 SIM 并行；未开机则 wait_npu）
  → 回收 FEEDBACK + logs → 刷新 KB/DAG/QUEUE
  → python3 scripts/check_rg_dag.py --yaml docs/rg-ascendc-engineering.yaml
  → python3 scripts/rg_viz.py
```

| 规则 | 说明 |
|------|------|
| 一刀一目录 | 坏了回退；实现在 `dec_related/RB-D*` |
| **NPU 主控专属** | Subagent TASK **不得**要求 `-r npu` / SSH；NPU 段由主控写 FEEDBACK 批注或另开 N* 刀 |
| **上板前先喊用户开机** | 云 NPU **默认未开**；主控需要 NPU 时 **先口头/书面请用户启动**，再 `which_npu`；未开则标 `wait_npu`，继续推 CPU/设计 |
| **占机 / 放机（用户锁定）** | **确定还要用**：主控开 `scripts/cannlab/agent_link_keepalive.sh` 定时心跳（建议 ≤40s），避免看门狗空闲断机。**确定不用**：停一切远端作业 + **停心跳**，约 **5–30 分钟**空闲后云机自断（以当次 bootstrap `IDLE_MIN` 为准）；勿空挂 keepalive |
| **刀间禁空占卡（强制，2026-09-09 事故）** | 任一 NPU 作业 **结束后 1 分钟内**必须：①写清结果；②**立刻开下一刀**或 **kill keepalive 放机**。禁止「回填已绿却只留心跳」。空占卡 = 烧钱；主控责任 |
| **目标未达不停手（强制，2026-09-09）** | 战役目标（正确+反卡死+沉淀）未完成前：**禁止**作业一结束就停下来「等用户指示」。须自动：下一实验 / 写 KB·DAG·notes / 刷新 QUEUE。仅当用户明确放机、或队列已空且沉淀已交，才停心跳 |
| **汇报口径（强制）** | 对用户说话禁止只报「D0x/K0x 完成」。必须写清：**测的是哪一步（人话）**、**对拍谁（liboqs/oracle）**、**结果（max=0 / ok=N fail=0 / 门禁是否关）**。代号可附括号 |
| **SIM 慢 → NPU 顶上** | 同实验不必卡在 SIM；**用户开机后**主控可先上 NPU 取真机证据 |
| **NPU 空闲可并行**（用户锁定） | 云机已开且卡空闲时，主控 **不必等本机 CPU/SIM 返回** 即可 rsync 上板同实验；与 subagent CPU **并行**。本机未绿不代表禁止先探 NPU；两边证据都写入 FEEDBACK |
| **有云机后勿以 CPU 为写码基线**（用户锁定，2026-09-09） | 云 NPU **已开可用**时：实现/排错/验收以 **`-r npu`** 为默认真相；**禁止**先只在 CPU 孪生上「写通再搬」当主路径（易引入仅 NPU host 才爆的 `-Werror`/ABI/同步问题，如 `AllocSz` unused）。CPU 仅作本机无卡时的辅助；Subagent 仍可跑 CPU，但主控须尽早并行 NPU，**编挂以 NPU 日志为准先修** |
| 失败一等公民 | 失败沉 KB + DAG |
| 禁否决 sync_audit | CPU/SIM 绿 ≠ 同步干净 |
| Subagent 可读 | KB 节 + DAG 节点；**禁改** yaml/kb |
| 禁抄 | inventory §4；含 **T25–T27 源码** |

---

## 6. 建议总序（设计后可调）

| 波 | 刀（建议 ID） | 内容 |
|----|---------------|------|
| **S0** | `DRW-S0A` / `DRW-S0B` | 设计评审：Decrypt 多 launch 拓扑；Decaps 刀序 + FO |
| **D1** | `DRW-D01…` | Decrypt prep / NTT+dot / INTT+extract |
| **D2** | `DRW-D1x` | Decrypt 全链 + liboqs |
| **K1** | `DRW-K01…` | Decaps G + Re-Encrypt + FO |
| **K2** | `DRW-RT` | 设备 Encaps↔Decaps + ×N |

---

## 7. 与 Encrypt 战役的切割

| Encrypt 战役 | Decrypt 战役 |
|--------------|--------------|
| 已收口，勿重开 T22–T24 | 新 ops + 新 KB/DAG |
| 反卡死检查单 **继承** | 另增 Decrypt：prep∥NTT 拆 launch；NTT∥INTT 拆核（见 Alg.15 原理 note） |
| Encaps G / pack DataCopy | Decaps 复用 **教训**（X12、撞名）不复用 **源码** |

---

**状态**：工作模式已锁定（2026-09-09）；**NPU=主控 / CPU·SIM=subagent**；S0 设计刀派发中。
