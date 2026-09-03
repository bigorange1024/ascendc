# graph_tests 总章程 — Encrypt 卡死排查（勿忘）

> **本文件是本排查线的人读真理源之一**（与图谱并列）。主控 / 新会话 / subagent 开工前必读。  
> **最后刷新**：2026-09-03  
> **Git**：不得自主 `commit` / `push` / 开分支；仅用户当次明确授权才可。

---

## 1. 用户要我做的事（总任务）

### 1.1 最终目标

在 **NPU 实机**上跑通 Encrypt：

- **不再卡死**在 `l18_l19`（及同类 MIX 挂点：`prep_ntt` / `ntt_y` 等）
- **正确性通过**（golden / 权威交叉）

对照：KeyGen 实机未出同类卡死；先改 **Encrypt**，Decaps（含 K=131）另线。

### 1.2 当前近程目标（未达最终前）

1. **不要**再以 stable 全量 Encaps + 大量哈希的慢 SIM 当主迭代路径。  
2. 在 **`ascendc-tests/`** 新建轻量探针：模仿 Encrypt **任务链骨架**（stub/轻量哈希、NTT、INTT、内积、encoding 等串接）。  
3. **不对算法正确性**；只要能**正常跑完**，且 **SIM 明显更快**。  
4. 用探针 / `graph_tests` 实验去验证图谱假说，逼近实机根因。  
5. **实验成功标准**：结论与改法必须能导向解决 **Encrypt 运行卡死**（不是「toy 自己绿了就交差」）。  
6. **实机**：用户可代跑；主控须先 **SIM 充分**再请上机。  
7. **当前进度（2026-09-03）**：TASK-001..006 闭环；stable Encaps 默认 Host 折 μ **SIM 绿**；**等用户 NPU 加压**（`D-await-npu-host-mu`）。  
8. **等 NPU 期间**：clean Encrypt 重写（`ENCRYPT_CLEAN_REWRITE.md` / TASK-007 P0）；Decaps K=131 另图（`DECRYPT_K131_PLAN.md` / TASK-008 排队）。

### 1.3 推理图谱（主控职责）

| 项 | 路径 / 约定 |
|----|-------------|
| 图谱 yaml | [`docs/rg-kem-encrypt-hang.yaml`](../docs/rg-kem-encrypt-hang.yaml) |
| Viewer HTML | [`docs/rg-kem-encrypt-hang.html`](../docs/rg-kem-encrypt-hang.html)（本地打开；**不自主推送**换公网链） |
| 工具 skill | `thirdparty/reasoning-graph-skill-master/`（**不**拷进 `.cursor/skills/`） |
| 协议模板 | [`docs/rg/AGENT_TASK_PROTOCOL.md`](../docs/rg/AGENT_TASK_PROTOCOL.md) |
| 谁写图 | **仅主控**刷新；subagent 默认可读、默认不改 yaml |

图谱作用：**沉淀有效知识 + 可推翻的推理**。推论被证伪 → 沿 `deps` **整链撤回/降级**，不能只堆成功。

### 1.4 图谱入库门禁（污染禁止）

| 必须收 | 禁止收 / 禁止当推荐 |
|--------|---------------------|
| 服务 **Encrypt 卡死 debug** 的断言 | 无关闲聊、流程导游、与本 bug 无关内容 |
| **失败实验 / 证伪假说 / 回退方案**（`retracted` / `inactive` + 证据） | 只沉淀「对拍绿」成功路径 |
| SIM / 实机可复现观测、合法同步约束 | 把「逐步外搬 GM」「滥增 Host launch」「标量碎写」等**严重伤效率/性能**的 correctness 捷径标成 **active 推荐**（负面对照可 `inactive` 留痕） |

验证口径（本排查线）：

- **最终**：NPU 实机  
- **迭代关键**：**SIM**（`SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4`）  
- **不跑 CPU 作门禁、不沉淀 CPU 孪生经验**

### 1.5 实验落点

| 目录 | 用途 |
|------|------|
| `graph_tests/` | 图谱假说的**小实验**（用户授权新建）；工单 inbox/outbox |
| `ascendc-tests/<探针>/` | Encrypt **任务链骨架 toy**（主近程交付物） |
| stable Encaps | **非**当前主改码场；全量慢 SIM 不作日常迭代 |

---

## 2. 分工

| 角色 | 做 | 不做 |
|------|----|------|
| **主控** | 定目标；维护/推理图谱；设计实验；写 TASK；读 FEEDBACK；决定下一刀；把控时间与止损 | 不自己堆实现代码（约定）；不并行多路 SIM；不自主推送 |
| **Subagent** | 按单份 TASK 干活；写 FEEDBACK；可读图谱摘录 | 不定方向；不擅自扩 scope；不改图谱（除非 TASK 写明）；不同时再启第二个干活 agent |

同一时间：**只允许一个 subagent 在干活**（尤其涉及 SIM）。

---

## 3. Subagent 时间与止损规则（主控强制执行）

细则见 [`SUBAGENT_RULES.md`](SUBAGENT_RULES.md)。摘要：

1. **单工**：同时最多 1 个执行中 TASK；前一单未收尾（FEEDBACK 或主控宣告 ABORT）不开下一单。  
2. **时限**：每份 TASK 必须写 `deadline_min`（墙钟建议）与 `max_retries`（默认 1）。  
3. **止损触发**（任一即主控 ABORT，写入图谱为失败经验，不无限返工）：  
   - 超过 `deadline_min` 仍无 FEEDBACK  
   - 同一错误循环改 ≥ `max_retries` 次仍无进展  
   - SIM/编译疑似挂死（无新日志超过约定静默窗口）  
   - subagent 偏离白名单改码  
4. **失败也要交卷**：ABORT/超时必须有 FEEDBACK（或主控代写），并刷新图谱（`refute` / `inactive`），**禁止干等**。  
5. **禁止并行 SIM**（仓库 Rule + 本线）：不得同时跑两路 `run.sh -r sim`。

---

## 4. 工单流

```
主控推理图谱 → 写 _inbox/TASK-*.md
    → 启唯一 subagent
    → _outbox/FB-*.md
    → 主控刷新图谱 → 下一刀或止损改假说
```

模板：[`docs/rg/AGENT_TASK_PROTOCOL.md`](../docs/rg/AGENT_TASK_PROTOCOL.md)

---

## 5. 当前状态与下一刀

| 项 | 状态 |
|----|------|
| 初始图谱 W0 | 已建；`rg_validate` OK；含失败路径与入库门禁 |
| `graph_tests/` | 已建；章程本文件 + 规则 |
| Encrypt 骨架 toy | **尚未**建；待首个 TASK |
| 实机粘性根因 | **未闭环**（`Q-root-cause` open） |
| Git 推送 | **禁止自主推送** |

**下一刀**：主控下发 `TASK-001`（建 `ascendc-tests` Encrypt 骨架 toy，SIM 通跑、快于全量），严格单工 + 时限。

---

## 6. 相关索引

- 交接：[`AGENT_HANDOFF.md`](../AGENT_HANDOFF.md)  
- qa：[`qa/2026-09/2026-09-03-Encaps-Decaps真2launch与GATE.md`](../qa/2026-09/2026-09-03-Encaps-Decaps真2launch与GATE.md)  
- 用例表：[`INDEX.md`](INDEX.md)
