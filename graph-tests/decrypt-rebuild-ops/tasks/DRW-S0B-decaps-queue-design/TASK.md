# DRW-S0B — Decaps 刀序与 FO/往返实验计划

| 字段 | 值 |
|------|-----|
| 状态 | ready / **dispatched** |
| DAG | `E-S0B-QUEUE` · `G-DG4..DG7` · `Q-DECAPS-CORRECT` · `Q-RT-HANG` |
| 代码目录 | **无**（本刀只设计） |
| 运营目录 | `graph-tests/decrypt-rebuild-ops/tasks/DRW-S0B-decaps-queue-design/` |
| 墙钟 | ≤ 25 min |
| runner | **subagent**（设计 only；**无 NPU**） |

## 目标

在 Decrypt 将按多 launch 重写的前提下，设计 **Decaps 编码实验总序**（DG4 G → DG5 Re-Encrypt → DG6 FO → DG7 设备往返+压测），写清每刀目录命名、依赖、权威交叉、与 Encaps 收口（T22–T24）的**接口契约**（非抄码）。

## 非目标

- 不写核；不修改 KB/DAG。  
- 不把临时 T26/T27 五/七 launch **源码**当模板（可读 FEEDBACK 中的 launch 数/失败类）。  
- 不重开 Encrypt/Encaps 功能刀。

## 必读（只读）

1. `docs/notes/Decrypt-cannbot-rebuild-capability-inventory.md` §2–§3  
2. `docs/notes/Decrypt-cannbot-rebuild-kb.md` §A2、§B、§C（X12/X13/X16）  
3. `docs/rg-decrypt-cannbot-rebuild.yaml`（`G-DG4..7`、`Q-*`、`E-S0B-QUEUE`）  
4. `docs/notes/Encrypt-cannbot-rebuild-kb.md` §A2、§B2（只读继承）  
5. `graph-tests/encrypt-rebuild-ops/tasks/T22-encaps-device-G/FEEDBACK.md`（若存在）思路  
6. `graph-tests/encrypt-rebuild-ops/tasks/T24-encaps-decaps-roundtrip/FEEDBACK.md` 压测口径  
7. `graph-tests/encrypt-rebuild-ops/tasks/T26-decaps-device/FEEDBACK.md`、`T27-…/FEEDBACK.md` **判决句 only**  
8. cannbot：`ascendc-sync-audit/SKILL.md`；可选 `ascendc-whitebox-design/SKILL.md` 浏览设计维度  
9. `graph-tests/decrypt-rebuild-ops/COMMON.md` · `QUEUE.md`

## 禁令

见 `COMMON.md`。禁建议「子进程调 liboqs 当生产 Decaps」；禁两段 `aclFinalize` 当生产基线（X16）。

## 验收（设计产出）

`FEEDBACK.md` 须含：

1. **刀序表**：建议 ID（`DRW-K01…`）、实现目录名 `RB-D*`、依赖、验收命令草案、liboqs 交叉点。  
2. **FO 两路径**：合法 / 拒绝如何造向量、如何避免假绿。  
3. **与 Decrypt 接口**：`m'`/`K'`/`coins`/`h`/`z` GM 契约（尺寸与生命周期）。  
4. **Re-Encrypt**：如何「接 Encaps 拓扑契约」而不抄 T22 源码（列出可复用契约条目）。  
5. **压测**：×N、预算 180s、失败分诊挂钩 deadlock-triage。  
6. **撞名检查单**：Decaps 合并 Encrypt+Decrypt 多核时的 basename 规则。  
7. **next_hint**：S0 之后主控应先派哪一刀。

## 回报

- 本目录 `FEEDBACK.md`（`ID: DESIGN_OK` / `DESIGN_BLOCKED`）。  
- 可选 `logs/queue-sketch.md`。  
- **禁止**改其它仓库文件。
