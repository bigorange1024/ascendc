# Agent 交接（KeyGen 重建 · NPU 收口后）

> **最后刷新**：2026-09-09（图谱+KB **推理重写**已合入工作区并校验）  
> **分支**：`chore/thirdparty-add-cannbot-skills`  
> **读者**：Local / 下一任 Agent  
> **入口**：本文件 → `AGENTS.md` → `docs/notes/ascendc-engineering-kb.md` → `docs/rg-ascendc-engineering.yaml` → `graph-tests/keygen-rebuild-ops/{COMMON,QUEUE}.md`

---

## ★ 当前真相（勿重新发明）

1. **Decrypt/Decaps 重建已收口**。运营：`graph-tests/decrypt-rebuild-ops/`。  
2. **KeyGen 重建（PKE+KEM）incubating 关闸**：`RB-K01…K06`；P04≡liboqs_pke；K02≡liboqs_kem；**NPU×30 三档 pass=30/fail=0/hang=0**。  
3. 关闸在图上仅薄指针 `J-REBUILD-TOPO-PASSES-GATES`（≠晋级 stable）；细节在 QUEUE/MATRIX，**不**再拆 `Q-KEYGEN-*` 战役题。  
4. **工程 KB/图谱**（用户批准后重写）：机制/证伪/权衡为主；战役流水退回 QUEUE；launch 表仅 KB 附录。校验：`check_rg_dag` + skill `rg_validate` OK（40 节点）。viz：`docs/rg-ascendc-engineering.viz.html`。  
5. K01 首轮 NPU 暴露：device 侧字符串字面量不可赋 `const char*` → 用 `constexpr uint8_t[]`（`F-DEVICE-STRING-GM`）。

---

## ★ 下一刀（非本战役默认）

| 若用户要… | 做 |
|-----------|-----|
| 晋级 `examples/stable-*` KeyGen | 须明确 `#交付#` + customspec；勿擅自从 graph-tests 抄进 stable |
| 继续 Local 开发 | 读 QUEUE（已 done）；可沉淀更多可复验写码事实进 KB |
| 再上板 | 先 `which_npu`；勿写死主机名；单卡串行 |

---

## 硬约束（摘要）

| 项 | 要求 |
|----|------|
| 禁抄 | KeyGen 算子级树；勿整文件 fork Encaps/Decrypt 核 |
| 同步 | flag∉{5,7}；禁 SoftSync；AIC Wait 环禁 SyncAll；NTT S1–S3 禁 limbsplit/Gather |
| 写出 | 业务 GM：UB+DataCopy |
| 权威 | liboqs；缺库 BLOCKED |
| Git | **无用户当次授权**不开分支 / 不 commit / 不 push |
| 设备字符串 | 禁 `const char* p = "…"` 于 `__aicore__`；用 `uint8_t` 表 |

细则：`graph-tests/keygen-rebuild-ops/COMMON.md`。

---

## 关键路径

| 用途 | 路径 |
|------|------|
| 短 KB | `docs/notes/ascendc-engineering-kb.md` |
| 图谱 | `docs/rg-ascendc-engineering.yaml` |
| KeyGen 队列 | `graph-tests/keygen-rebuild-ops/QUEUE.md` |
| 实现 | `graph-tests/kg_related/RB-K*` |
| NPU 证据 | `/mnt/workspace/keygen-npu-logs/`（远端） |
