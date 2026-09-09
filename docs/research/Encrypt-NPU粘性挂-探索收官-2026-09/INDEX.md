# Encrypt / Encaps NPU 粘性挂 · 探索收官（2026-09）

> **状态**：**路线关闭（就「在既有 l18/Encrypt 上继续 debug」而言）**  
> **原因**：多轮 SIM/NPU 未钉可操作根因；用户决定 **全新编写 PKE/KEM**，本目录仅保留有价值的探索资产与结论边界。  
> **继任**：新算子实现 + **验收/测试**（勿把本目录当「下一刀改哪行」的施工单）。  
> **性质**：`docs/research/` 收官包；权威过程细节仍以原路径为准，此处给 **一张图 + 指针**。

---

## 1. 一句话结论

实机粘性挂真实存在，挂面在 Encaps **`launch 2 f203_encrypt_l18_l19` 的 Host Sync**；末段结构已钉为 **AIC 早退后 AIV-only 尾（零 CrossCore）**；SIM 证明该结构可活但 **不能复现粘性**；**根因未钉死**，继续在旧码上改已不划算。

---

## 2. 已证实（可带进新实现的「别再踩」）

| ID | 结论 |
|----|------|
| N9–N10 | 挂在 `l18_l19` Sync；冷启可 ×10 后挂；污染可首轮挂 |
| N12 | l18 FSM stub / INTT flag 复用 **不是**粘性充分条件（E17/E18 NPU 绿） |
| N15–N17 | 空 TRACE 多为 **标量写回观测假象**；DataCopy 后挂时可见 **16/16** → 挂窗偏 **全 Mark 之后** |
| N18–N19 | TRACE-DC **抬失败/挂率**（诊断 opt-in）；干净卡仍复现冷启晚轮挂 |
| E21 | INTT `PACK` 后 AIV：`merge → mod_q → tail_pack`，**零 CrossCore** |
| E20 | 同构 stub SIM×8 可活 → 「末段结构非法」不成立 |
| 方法 | 猎挂 **首挂即停**；禁长 SSH；`which_npu` 自动发现；SIM ≠ 粘性 |

---

## 3. 明确未解决

- Sync 不回时设备卡在 **哪条等待 / 是否仅 AIV 尾未退 / Runtime 黑盒**  
- 何种 **可证伪改法**（如 tail 外置 launch）能消挂 —— **未做**（已停止在旧码上试）

---

## 4. 对新写 PKE/KEM 的约束建议（非规格）

1. **Host launch / Sync 边界清晰**：避免「AIC 早已退出、AIV 长尾仍占同一 MIX launch」成为默认唯一形态时，缺少可观测退出契约。  
2. **观测探针默认关**：整表 TRACE/DataCopy RMW 仅诊断；生产路径勿默开。  
3. **验收分层**：CPU → SIM →（授权）NPU；**勿**用 SIM 绿证明「无粘性」。  
4. **NPU 猎挂**：`timeout≈240` 只确认「本轮是挂」；默认首挂停。  
5. **禁**把本收官包内 toys/旧 encaps 当模板抄进新算子（可读判决与结论表即可）。

---

## 5. 资产索引（原件位置）

### 5.1 定稿/半定稿知识

| 资产 | 路径 |
|------|------|
| 领域知识库（N1–N19） | [`docs/notes/Encrypt-实机无卡死-知识库.md`](../../notes/Encrypt-实机无卡死-知识库.md) |
| 推理图谱 | [`docs/rg-encrypt-npu-hangfree.yaml`](../../rg-encrypt-npu-hangfree.yaml) |
| ENCRYPT-GAP 差清单 | [`graph_tests/ENCRYPT_GAP.md`](../../../graph_tests/ENCRYPT_GAP.md) |
| 空 TRACE 修订 | [`graph_tests/EARLY_EMPTY_TRACE.md`](../../../graph_tests/EARLY_EMPTY_TRACE.md) |
| CANNLab 远程/防断连 | [`docs/engineering/CANNLab接入与远程驱动.md`](../../engineering/CANNLab接入与远程驱动.md) |

### 5.2 关键 FEEDBACK（实验证据）

| 文件 | 要点 |
|------|------|
| [`FEEDBACK-E17/E18`](../../../graph_tests/_outbox/) | SIM+NPU：reuse flag 非充分 |
| [`FEEDBACK-E19/E19b`](../../../graph_tests/_outbox/) | 标量丢槽 vs DataCopy 稳 |
| [`FEEDBACK-NPU-ENCAPS-TRACE-DC`](../../../graph_tests/_outbox/FEEDBACK-NPU-ENCAPS-TRACE-DC.md) | 挂时 16/16 |
| [`FEEDBACK-NPU-AB`](../../../graph_tests/_outbox/FEEDBACK-NPU-AB.md) | 污染卡 A/B |
| [`FEEDBACK-NPU-AB-CLEAN`](../../../graph_tests/_outbox/FEEDBACK-NPU-AB-CLEAN.md) | 干净卡；首挂政策 |
| [`FEEDBACK-E20/E21`](../../../graph_tests/_outbox/) | 末段 stub / 零 CC 审计 |

本目录 [`evidence/`](evidence/) 含上述关键 FEEDBACK 的 **副本**（收官只读快照；更新以 `_outbox` 为准）。

### 5.3 Toys / 脚本（工程资产，非业务规格）

| 资产 | 路径 |
|------|------|
| E17–E21 toys | `graph_tests/toys/toy-e17-*` … `toy-e20-postmark-tail` |
| 远程 nohup / 首挂停 | `scripts/cannlab/run_npu_ab_nohup.sh` 等 |
| 自动发现主机 | `scripts/cannlab/which_npu.sh` |

### 5.4 纪要

| 日期 | 路径 |
|------|------|
| 2026-09-07 | `qa/2026-09/2026-09-07-NPU套件单轮全绿与RxN.md` |
| 2026-09-08 | `qa/2026-09/2026-09-08-防断连与NPU-AB投递.md` |
| 2026-09-09 | `qa/2026-09/2026-09-09-粘性挂探索收官与转向新写.md` |

---

## 6. 关闭声明

**自 2026-09-09 起**：不在既有 `stable-…-kem-encaps-k4` / `f203_encrypt_l18_l19` 上再开挂因 debug 刀。  
本包供新写与验收时 **查阅边界**；实现以新 customspec / 新目录为准。
