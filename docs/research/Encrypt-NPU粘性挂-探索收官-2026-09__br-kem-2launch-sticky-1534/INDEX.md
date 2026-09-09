# Encrypt / Encaps NPU 粘性挂 · 探索收官（2026-09）

> **来源分支**：`cursor/kem-2launch-sticky-1534`  
> **本目录名后缀**：`__br-kem-2launch-sticky-1534`（多分支并存时用于区分）  
> **状态**：**路线关闭（就「在既有 l18/Encrypt 上继续 debug」而言）**  
> **继任**：全新编写 PKE/KEM + **验收/测试**  
> **合入 main**：见文末清单；**`scripts/cannlab/*` 不合入 main**（仅留本分支）。

---

## 1. 一句话结论

实机粘性挂真实存在，挂面在 Encaps **`launch 2 f203_encrypt_l18_l19` 的 Host Sync**；末段为 **AIC 早退后 AIV-only 尾（零 CrossCore）**；SIM 可活但 **不能复现粘性**；**根因未钉死**。

---

## 2. 已证实（禁踩）

| ID | 结论 |
|----|------|
| N9–N10 | 挂在 `l18_l19` Sync；冷启可 ×10 后挂；污染可首轮挂 |
| N12 | l18 FSM stub / flag 复用 **非**充分条件 |
| N15–N17 | 空 TRACE 多为观测假象；挂时可见 **16/16** → 窗偏全 Mark 后 |
| N18–N19 | TRACE-DC 抬失败/挂率（opt-in）；干净卡仍晚轮挂 |
| E21 | INTT `PACK` 后 AIV 尾 **零 CrossCore** |
| E20 | 同构 stub SIM 可活 ≠ 粘性复现 |

---

## 3. 未解决

Sync 不回时设备卡点；可证伪改法（如 tail 外置 launch）**未做**（已停旧码试错）。

---

## 4. 对新写算子的约束建议（非规格）

1. Host launch / Sync 退出契约清晰  
2. TRACE 探针默认关  
3. 验收：CPU → SIM →（授权）NPU；SIM 绿 ≠ 无粘性  
4. 猎挂默认首挂即停  
5. 禁抄本包 toys / 旧 encaps 当模板  

---

## 5. 本包内证据快照

目录：[`evidence__br-kem-2launch-sticky-1534/`](evidence__br-kem-2launch-sticky-1534/)

| 快照 | 要点 |
|------|------|
| `ENCRYPT_GAP.md` / `EARLY_EMPTY_TRACE.md` | 差清单 / 空 TRACE 修订 |
| `FEEDBACK-NPU-AB.md` / `FEEDBACK-NPU-AB-CLEAN.md` | 污染 vs 干净对照 |
| `FEEDBACK-NPU-ENCAPS-TRACE-DC.md` | 挂时 16/16 |
| `FEEDBACK-E19.md` / `E19b` / `E21` | 观测假象 / 末段零 CC |
| 其余 E17/E18/E20 | 过程证据（合 main 可省略，见下） |

权威过程件仍在 `graph_tests/_outbox/`（**建议不合 main**）。

---

## 6. 合入 main 清单（本分支标注）

### 建议合入

| 路径 |
|------|
| `docs/research/Encrypt-NPU粘性挂-探索收官-2026-09__br-kem-2launch-sticky-1534/`（本包；evidence 可只留 INDEX 点名的关键 FEEDBACK） |
| `docs/notes/Encrypt-实机无卡死-知识库.md` + `docs/notes/INDEX.md` |
| `docs/research/INDEX.md` |
| `docs/rg-encrypt-npu-hangfree.yaml` |
| `docs/engineering/CANNLab接入与远程驱动.md`（文档即可） |
| `qa/2026-09/2026-09-08-防断连与NPU-AB投递__br-kem-2launch-sticky-1534.md` |
| `qa/2026-09/2026-09-09-粘性挂探索收官与转向新写__br-kem-2launch-sticky-1534.md` |
| 对应 `qa/INDEX.md` / `qa/2026-09/INDEX.md` 行 |

### 明确不合入 main

| 类别 | 说明 |
|------|------|
| **`scripts/cannlab/*`** | 用户裁定：**脚本不合 main**（含 lib_ssh / which_npu / keepalive / remote_job / run_npu_ab_nohup） |
| `graph_tests/toys/**`、`_inbox/TASK-*`、`_outbox/FEEDBACK-*` | 测试/过程噪音 |
| encaps `f203_encrypt_l18_l19_kernel.cpp` TRACE-DC 改码 | 旧路线诊断码 |
| `AGENT_HANDOFF.md` | 交接草稿，按 main 惯例通常不带 |

---

## 7. 关闭声明

自 2026-09-09：不在既有 encaps/`l18` 上再开挂因 debug。本包供查阅边界；实现以新 customspec / 新目录为准。
