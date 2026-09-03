# graph_tests 总章程 — MIX 卡死排查（Encrypt / Decrypt）

> **本文件是排查线的人读真理源之一**（与图谱并列）。主控 / 新会话 / subagent 开工前必读。  
> **最后刷新**：2026-09-03  
> **Git**：干活 subagent 不得自主 `commit` / `push`；主控按当次授权处理。

---

## 1. 用户要我做的事（总任务）

三张图、三条线，**禁止混成一张交差**：

| 线 | 图谱 | 近程 |
|----|------|------|
| Encaps 粘性 | `docs/rg-kem-encrypt-hang.yaml` | Hostμ SIM 绿；等 NPU；clean P0 已绿 |
| **PKE Decrypt 卡死** | `docs/rg-kem-decrypt-hang.yaml` | **当前主控焦点**：toy 沉 SoftSync/GATE 机制，**未沉前不上机** |
| Decaps K≠0 | `docs/rg-kem-decrypt-k131.yaml` | Cloud SIM 仍绿；K 错与 hang 正交 |

### 1.1 最终目标

- **Encrypt**：NPU 上不再卡死在 `l18_l19` / `prep_ntt` 等，且正确性通过。  
- **Decrypt hang**：NPU 上 `stable-fips203-mlkem-pke-decrypt-k4` fused 能跑完且 `m` 对拍。  
- **Decaps K**：另图；不要把 hang TASK 写成修 K=131。

### 1.2 当前近程目标（未达最终前）

1. **不要**用 stable 全量慢 SIM 当 hang 主迭代。  
2. **Decrypt hang（当前）**：`fix-decrypt-skel-mix-chain-toy` 模仿 fused **SoftSync + 两轮 GATE + stub Cube**；故障注入沉积挂死机制。  
3. Encrypt hang toy 已绿（缺 SET(4)⇒124）；不再空转全量 Encaps。  
4. **不对算法正确性**；要能跑完或按设计 124。  
5. **实验成功标准**：导向消除 **对应线** 的运行卡死（Decrypt hang 或 Encaps hang），不是 toy 自洽绿交差。  
6. **实机**：SIM 充分沉机制后再请上机；Decrypt hang **现在还不够格上机**。  
7. Encaps：TASK-001..007 闭环；Hostμ SIM 绿；clean P0 绿。  
8. Decaps K=131：TASK-008 SIM 绿；与 hang 分图。

### 1.3 推理图谱（主控职责）

| 项 | 路径 / 约定 |
|----|-------------|
| 图谱 yaml | Encrypt hang / **Decrypt hang** / Decaps K 三份，见 [`INDEX.md`](INDEX.md) |
| Viewer HTML | 同名 `.html`（本地打开） |
| 工具 skill | `thirdparty/reasoning-graph-skill-master/`（**不**拷进 `.cursor/skills/`） |
| 协议模板 | [`docs/rg/AGENT_TASK_PROTOCOL.md`](../docs/rg/AGENT_TASK_PROTOCOL.md) |
| 谁写图 | **仅主控**刷新；subagent 默认可读、默认不改 yaml |

图谱作用：**沉淀有效知识 + 可推翻的推理**。推论被证伪 → 沿 `deps` **整链撤回/降级**，不能只堆成功。

### 1.4 图谱入库门禁（污染禁止）

| 必须收 | 禁止收 / 禁止当推荐 |
|--------|---------------------|
| 服务 **当前 TASK 所属 hang 线** debug 的断言 | 无关闲聊、把 K=131 写进 hang 图、把 Encaps 热修当 Decrypt 充分解 |
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
| `ascendc-tests/fix-encrypt-skel-mix-chain-toy/` | Encrypt 骨架 toy（已绿；缺 SET(4)⇒124） |
| `ascendc-tests/fix-decrypt-skel-mix-chain-toy/` | **Decrypt fused 握手 toy**（TASK-009 起） |
| stable Encaps / PKE Decrypt | **非**日常改码场；全量慢 SIM 不作 hang 迭代 |

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
| Encrypt hang 图 | TASK-001..007 已沉；等 NPU Hostμ |
| **Decrypt hang 图** | **T0–T4 齐**；CrossCore 缺 SET(4)⇒124；空/脏 SoftSync 非 SIM hang |
| Decrypt 骨架 toy | TASK-009..012 闭环 |
| graph_tests | 下一候选 TRACE |
| Decaps K 图 | TASK-008 SIM 绿；与 hang 正交 |
| Git | 干活 subagent 禁止自主推送 |

**下一刀候选**：Decrypt 设备 TRACE 层标记（先 SIM）；**不要**默认请用户上 Decrypt NPU。

---

## 6. 相关索引

- 交接：[`AGENT_HANDOFF.md`](../AGENT_HANDOFF.md)  
- qa：[`qa/2026-09/2026-09-03-Encaps-Decaps真2launch与GATE.md`](../qa/2026-09/2026-09-03-Encaps-Decaps真2launch与GATE.md)  
- 用例表：[`INDEX.md`](INDEX.md)
