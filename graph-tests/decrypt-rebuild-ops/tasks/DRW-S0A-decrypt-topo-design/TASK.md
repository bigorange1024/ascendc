# DRW-S0A — Decrypt 多 launch 拓扑设计评审

| 字段 | 值 |
|------|-----|
| 状态 | ready / **dispatched** |
| DAG | `E-S0A-TOPO` · 相关 `C-DEC-SPLIT` `C-ANTI-HANG` `G-DG1..DG3` |
| 代码目录 | **无**（本刀只设计） |
| 运营目录 | `graph-tests/decrypt-rebuild-ops/tasks/DRW-S0A-decrypt-topo-design/` |
| 墙钟 | ≤ 25 min |
| runner | **subagent**（设计 only；**无 NPU**） |

## 目标

基于 cannbot CrossCore / sync-audit 约束 + 本仓反卡死检查单 + Alg.15 **原理** note，给出 **Decrypt 设备多 launch 拓扑草案**（prep AIV → mid-sync → NTT/dot MIX → sync → INTT/extract），明确每 launch 的核类型、flag 集合、I/O GM、验收分段。

## 非目标

- **不写**任何 `.cpp/.hpp` 实现；不创建 `dec_related` 目录。  
- 不修改 KB/DAG yaml。  
- 不打开 / 照抄 `RB-T25` 或 alg15 源码（可读其 FEEDBACK 判决句）。

## 必读（只读）

1. `docs/notes/Decrypt-cannbot-rebuild-capability-inventory.md`  
2. `docs/notes/Decrypt-cannbot-rebuild-kb.md` §B2、§C（X12–X15）  
3. `docs/rg-decrypt-cannbot-rebuild.yaml`（节点 `C-DEC-SPLIT`、`G-DG1..3`、`E-S0A-TOPO`）  
4. `docs/notes/MIX-Encrypt-Encaps-反卡死拓扑技术总结.md` §5 检查单  
5. `docs/notes/F203-Alg15-Decrypt-2launch编排技术总结.md`（**原理**；禁对照生产源码）  
6. cannbot：  
   - `thirdparty/cannbot-skills/ops/ascendc-api-best-practices/references/api-crosscore-sync.md`  
   - `thirdparty/cannbot-skills/ops/ascendc-sync-audit/SKILL.md`  
   - `thirdparty/cannbot-skills/ops/ascendc-sync-audit/workflows/deadlock-triage.md`（浏览挂死分诊维度）  
7. `graph-tests/decrypt-rebuild-ops/COMMON.md`

## 禁令

见 `COMMON.md`。尤其：禁抄 T25 源码；禁建议 SoftSync / flag 5·7 / prep∥NTT 同 launch。

## cannbot 用法（本刀）

- 用 api-crosscore-sync + sync-audit **规则**审查你提出的拓扑（文字即可）。  
- 列出若编码后 `sync_audit` 最可能触发的检查项（预防表）。  
- **不必**对空目录跑 sync_audit.py。

## 验收（设计产出）

在 `FEEDBACK.md` 写清：

1. **Launch 表**：序号、核名建议（含 **唯一 basename**）、AIV-only vs MIX、输入/输出 GM、是否 CrossCore、flag∈{1,3,4?}。  
2. **为何拆**：对照 X14/X15 / 反卡死 I1。  
3. **分段验收**：每 launch 后可对拍的张量与 oracle（liboqs/host）。  
4. **风险**：507000 / 假绿 / 半成品 û 的监测点。  
5. **对 DG1–DG3 的映射**：建议第一编码刀切在哪。  
6. **next_hint**：给主控的 D01 TASK 草稿要点（≤10 行）。

## 回报

- 写本目录 `FEEDBACK.md`（`ID: DESIGN_OK` 或 `DESIGN_BLOCKED`）。  
- 可选：`logs/topo-sketch.md`。  
- **禁止**改仓库其它文件（除本任务 FEEDBACK/logs）。
