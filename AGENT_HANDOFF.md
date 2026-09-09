# Agent 交接（给下一任 Cloud Agent）

> **最后刷新**：2026-09-09（Local 下班交接；已推 `chore/thirdparty-add-cannbot-skills`）  
> **读者**：Cursor **Cloud Agent**（可经 Tailscale 驱动 CANNLab NPU）  
> **入口**：先读本文件 → `AGENTS.md` → `docs/notes/ascendc-engineering-kb.md` → `docs/rg-ascendc-engineering.yaml`

---

## ★ 当前真相（勿重新发明）

1. **Decrypt/Decaps 重建已收口**（正确 + NPU×30/≈100 不挂）。运营只读：`graph-tests/decrypt-rebuild-ops/`。  
2. **KeyGen 重建 CPU 全链齐**：`graph-tests/kg_related/RB-K01…K06`；P04≡`liboqs_pke_ref`；K02≡`liboqs_kem_ref`（ek/dk_kem max=0）。  
3. **全部 KeyGen NPU 仍为 `wait_npu`**；门禁 `Q-KEYGEN-HANG` / `Q-KEYGEN-CORRECT` **open**（CPU 绿不关卡死门）。  
4. **工程 KB/图谱章程**（2026-09-09 定）：  
   - 目标经验包 = **正确 ∧ 不卡** 的 ML-KEM AscendC；**反卡死是重点风险，不是唯一内容**。  
   - 知识 = 可反复检验的经验；**用法拼法 / 刀号 PASS = 流水账**（只写 QUEUE/HANDOFF/MATRIX）。  
   - 图谱校验：`python3 scripts/check_rg_dag.py && python3 scripts/rg_viz.py`

---

## P0（你接手当做的事）

### 若用户已启动 CANNLab 云机

1. 按 [`docs/engineering/CANNLab接入与远程驱动.md`](docs/engineering/CANNLab接入与远程驱动.md) 连上 `cannlab-npu:2222`（Secrets：`TAILSCALE_AUTHKEY`、`CANNLAB_SSH_KEY`）。  
2. 远端 `git pull` 本分支后，对下列目录做 **干净卡 NPU×30**（预算默认 **180s/次**；超时=缺陷）：  
   - `graph-tests/kg_related/RB-K04-pke-full/`（P04）  
   - `graph-tests/kg_related/RB-K05-kem-tail/`（K01）  
   - `graph-tests/kg_related/RB-K06-kem-full/`（K02，优先关 KEM 全链）  
3. 回写：`keygen-rebuild-ops/{QUEUE,MATRIX,NPU_LIVE}.md`、各刀 `FEEDBACK.md`、本 HANDOFF。  
4. 若 **挂死**：按 fe53/1534 风格把可再测事实写入 `docs/rg-ascendc-engineering.yaml` + 短 KB，再 `check_rg_dag` / `rg_viz`；**禁止**只改超时蒙混。  
5. 若 **I/O 错**：先答假绿三问（golden 同源？权威交叉？跨核 Sync？），再改码；正确性以 **同 ISA liboqs** 为准（禁 rsync x86 ref）。  
6. 两门都绿 → 关图上 `Q-KEYGEN-HANG` / `Q-KEYGEN-CORRECT`（及 QUEUE 对应行）；心跳文件按 Decrypt 战役惯例，刀间勿空烧，收工停心跳放机。

**估时（顺利）**：约 40–90 min；触顶 180s 挂死则显著更长。

### 若云机未开 / 连不上 NPU

- **禁止**把 CPU/SIM 绿写成 NPU 通过。  
- 可继续沉淀 **正确∧不卡** 写码经验到短 KB + 图谱（采样/CBD、ByteEncode、FO、KeyGen 数据契约等——只收可再测条目，不收刀号流水）。  
- 在 HANDOFF / QUEUE 标明 `blocked_cloud`，等用户开机。

---

## 硬约束（摘要）

| 项 | 要求 |
|----|------|
| 禁抄 | KeyGen 算子级树（stable/pass-fix/frozen/`f203_keygen_*`）；勿整文件 fork Encaps/Decrypt 核冒充 KeyGen |
| 同步 | flag∉{5,7}；禁 SoftSync；AIC Wait 环禁 SyncAll；NTT S1–S3 禁 limbsplit/Gather |
| 写出 | 业务 GM：UB+DataCopy（禁 SetValue→GM） |
| basename | 同 binary 内 `.cpp` basename 全局唯一 |
| 权威 | liboqs；缺库 BLOCKED，禁 python 冒充权威绿 |
| Git | **无用户当次授权**不开分支 / 不 commit / 不 push（本轮 Local 已推一版；你改完先汇报，等授权再推） |
| 角色 | 你是 **主控**（可 NPU）；勿再派 subagent 去 SSH/NPU |

运营细则：`graph-tests/keygen-rebuild-ops/COMMON.md`。

---

## 关键路径

| 用途 | 路径 |
|------|------|
| 短 KB | `docs/notes/ascendc-engineering-kb.md` |
| 图谱 | `docs/rg-ascendc-engineering.yaml` · `docs/rg-ascendc-engineering.viz.html` |
| KeyGen 队列 | `graph-tests/keygen-rebuild-ops/QUEUE.md` |
| 实现 | `graph-tests/kg_related/RB-K*` |
| 反卡死长文 | `docs/notes/MIX-Encrypt-Encaps-反卡死拓扑技术总结.md` · `MIX-Decrypt-Decaps-反卡死拓扑技术总结.md` |
| NTT 契约 | `docs/notes/MLKEM-NTT-实现总结.md` · `MLKEM-NTT-向量与标量实现指南.md` |
| Cloud VM 坑 | `Cursor-Cloud环境说明.md`（**无本地 NPU**；真机走 CANNLab） |
| 当日 qa | `qa/2026-09/2026-09-09-Decrypt-Decaps-cannbot重建脚手架.md` |

---

## 粘贴给 Cloud Agent 的短 Prompt

```text
先读 AGENT_HANDOFF.md、AGENTS.md、docs/notes/ascendc-engineering-kb.md、
docs/rg-ascendc-engineering.yaml、graph-tests/keygen-rebuild-ops/{COMMON,QUEUE}.md。
P0：用户开 CANNLab 后，主控对 RB-K04/K05/K06 做 NPU×30（正确∧不卡）；回写 QUEUE/MATRIX/NPU_LIVE/HANDOFF。
挂死→刷新工程 yaml+短 KB；勿假绿；禁抄 KeyGen 算子树；无授权不 commit/push。
云未开：blocked_cloud，可补 KEM 写码经验入库，禁止声称 NPU 通过。
```
