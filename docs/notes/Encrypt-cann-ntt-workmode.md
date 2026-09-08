# Encrypt × cann-ntt · 工作模式清单

> **主控本会话**：定实验、写任务书、用 cannbot-skills **设计**实验、派 subagent、回收反馈、刷新 KB/DAG。  
> **主控禁止**：写/改 kernel、用例实现、CMake 业务逻辑（任务书与文档除外）。  
> **Subagent**：按任务书编码 + CPU/SIM + cannbot 门禁；可读本线 KB/DAG/能力清单。  
> **配套**：[`Encrypt-cann-ntt-kb.md`](Encrypt-cann-ntt-kb.md) · [`Encrypt-cann-ntt-capability-inventory.md`](Encrypt-cann-ntt-capability-inventory.md) · [`../rg-encrypt-cann-ntt.yaml`](../rg-encrypt-cann-ntt.yaml)

---

## 0. 已锁定（用户 2026-09-08）

| 项 | 决定 |
|----|------|
| 路线 | Host 多段编排 + **迁入工程的 cann-ntt NTT/INTT**（有用代码搬进本仓用例树，非外部 ACLNN 黑盒依赖） |
| 参数 | ML-KEM-**1024** |
| 目标 | **绝对不卡死**；正确性非主门禁 |
| 落点 | `graph-tests/`（本线目录：`graph-tests/enc_cann_ntt/`） |
| 旧 hang 线 | ER01–05 / hang-KB **归档可读**；本线 **新 KB + 新 DAG** |
| Git | 无用户授权不 commit/push/开分支 |

---

## 1. 角色

| 角色 | 做 | 不做 |
|------|----|------|
| **主控** | 遍历能力清单+KB+DAG；用 cannbot skill **设计**下一刀；写 `ENxx-TASK.md`；派 Task subagent；审反馈；回写 KB/DAG/INDEX/HANDOFF | 不写核；不否决 sync_audit 红线；不擅自上机 |
| **Subagent** | 一刀一目录编码；跑验收；跑 cannbot 脚本；交短反馈（路径/命令/结论/红线） | 不定目标；不改冻结树；不抄禁路径；不擅自扩 scope |
| **用户** | 拍板路线；授权上机/Git；卡死讨论拍板 | 不填复杂表 |

---

## 2. 单刀生命周期（强制顺序）

```text
① 主控：读能力清单 + KB + DAG（失败条目优先）
② 主控：加载本刀所需 cannbot skills（见 §3）→ 写出实验设计要点
③ 主控：落盘 ENxx-TASK.md（目标/禁令/验收/时限/可读路径）
④ 主控：派 subagent（prompt 含 KB/DAG/TASK 绝对路径）
⑤ Subagent：编码 → CPU → SIM_DIRECT sim → sync_audit（若有设备同步）
⑥ Subagent：回报（STATUS + 日志要点）；超时须中止回报
⑦ 主控：审；沉淀 **成功与失败** 进 KB；DAG 挂节点；定下一刀或停
```

**一刀一目录**；坏了回退干净节点；**禁止**在红基础上叠刀。

---

## 3. cannbot-skills 用法（设计实验 · 不 vendor）

路径根：`thirdparty/cannbot-skills/`（**不**链入 `.cursor/skills`）。

| 阶段 | 主控必载（设计/门禁） | Subagent 执行 |
|------|----------------------|---------------|
| 开刀设计 | `ops/ascendc-tiling-design`、`ops/ascendc-api-best-practices`（含 crosscore）、`ops/npu-arch`（按需） | — |
| 工程壳 | `ops/ascendc-direct-invoke-template`（对照壳，勿整仓 init） | 可按任务书复制本仓 graph-tests 壳 |
| 每刀同步 | `ops/ascendc-sync-audit` | `scripts/sync_audit.py` + 有则 flow analyzer；**禁否决红线** |
| 卡死分诊 | `ops/ascendc-sync-audit` → deadlock-triage；`ops/ascendc-crash-debug`；`ops/ascendc-runtime-debug` | 按任务书跑脚本/贴结论 |
| 环境 | `ops/ascendc-env-check`、`ops/cann-env-setup`（文档） | 环境异常标 BLOCKED |
| 精度（后置） | `ops/ascendc-precision-debug` | **卡死未清前不启为主门禁** |
| 性能（后置） | `ops/ops-profiling` 等 | 同左 |

过程落盘（可选）：`.cannbot/mlkem-encrypt-cann-ntt/`（与旧 `.cannbot/mlkem-pke-encrypt/` 分开）。

**禁止**：未授权跑会改写根 `AGENTS.md` 的完整 `init.sh`。

---

## 4. 任务书最低字段（主控）

每个 `graph-tests/enc_cann_ntt/ENxx-TASK.md` 须含：

1. **目标一句话**（与不卡死的关系）  
2. **目录名**（新建；勿改旧 ER/Encrypt）  
3. **允许阅读**：能力 ID、shared、单功能 pass、KB/DAG、cann-ntt 源（迁入用）  
4. **禁止阅读/抄码**：Encrypt/KEM 算子目录、frozen 源码、旧 ER 核作模板  
5. **实现约束**：Host 段划分；NTT 必须用迁入积木；禁胖 MIX GATE  
6. **验收**：`bash run.sh -r cpu` + `SIM_DIRECT=1 bash run.sh -r sim`；无 stray dump；sync_audit 红线策略  
7. **时限**（墙钟上限）与超时动作  
8. **反馈格式**：PASS/FAIL/BLOCKED + 命令 + 红线条数 + 拟写入 KB 的一句教训  

---

## 5. Subagent 反馈最低字段

- 目录路径、关键文件列表  
- CPU / SIM 退出码与是否对拍（本阶段可「不对正确性」但须写明）  
- sync_audit：红线数 + json 路径  
- **失败优先**：挂/超时/红线原文  
- 未做项 / 阻塞（缺 CANN、缺矩阵 bin 等）

---

## 6. 知识沉淀纪律

| 做 | 不做 |
|----|------|
| 每刀后更新 [`Encrypt-cann-ntt-kb.md`](Encrypt-cann-ntt-kb.md)（短条目，失败优先） | 流水账、大段贴码、与 Encrypt 无关的性能长文 |
| 每刀挂/关 [`rg-encrypt-cann-ntt.yaml`](../rg-encrypt-cann-ntt.yaml) 节点 | 同质刀重复开（DAG 已否决的） |
| 允许 subagent **读** KB+DAG | 让 subagent **改** KB/DAG（除非任务书明示代写 STATUS） |
| 正确实验与失败实验都进 KB | 只报绿不报坑 |

旧 hang 库 [`Encrypt-hang-rewrite-kb.md`](Encrypt-hang-rewrite-kb.md)：**只读继承约束**（如禁 INTT 5/7）；新结论写本线 KB，避免两库双写膨胀。

---

## 7. 验收口径（本阶段）

| 级别 | 要求 |
|------|------|
| 刀通过 | CPU + `SIM_DIRECT=1` sim **跑完不挂**；用例根无 stray dump；任务书要求的 sync 门禁满足 |
| 正确性 | **非主门禁**；有 golden 可记但不阻塞「不卡死」主线 |
| 声称本线 Encrypt 可用 | 须 Host 全段串起来且 NPU（用户授权后）不 SynchronizeStream 卡死 |

---

## 8. 与 hang 重写线关系

| 线 | 目录 | KB / DAG |
|----|------|----------|
| 旧 | `graph-tests/enc_related/` ER01–05 | `Encrypt-hang-rewrite-kb.md` / `rg-encrypt-hang-rewrite.yaml`（**暂停加压**） |
| **本线** | `graph-tests/enc_cann_ntt/` ENxx | 本文档套件 |

不得把 ER 核 copy 成 EN 起点；可引用 KB 中 X25–X29 等**结论句**。
