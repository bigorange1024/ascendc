# AscendC 工程知识库（短 KB）

> **目标经验包**：写出 **正确且不卡死** 的 ML-KEM AscendC 算子。  
> **知识**＝可再测的机制 / 证伪教训 / 带代价的设计权衡。  
> **不是知识**＝战役现象台账、刀号×30、launch 拼法日记（→ QUEUE / MATRIX / HANDOFF）。  
> **图谱**：[`docs/rg-ascendc-engineering.yaml`](../rg-ascendc-engineering.yaml)（2026-09-09 **按推理重写**）· `python3 scripts/rg_viz.py`  
> **长文**：NTT 契约 · MIX 反卡死总结 · DataCopy 等 notes  

---

## 0. 三条作用

| # | 作用 |
|---|------|
| 1 | 写码时能想起机制与权衡，而不是回想「哪天哪刀绿了」 |
| 2 | 实验冲突 → 改推理链（`retracted`/`inactive`），不是堆新现象节点 |
| 3 | 换 Agent 只靠 KB+图应能推出「怎么写才对且不挂」 |

**双门禁**：权威 I/O 正确 ∧ NPU 加压不挂；证其一不蕴含另一。

---

## 1. 机制（先记住这些）

1. **跨核挂 ≈ 已 `Wait(f)` 而对端本次不可达 `Set(f)`**（含 Wait 环内再阻塞）。修的是可达性，不是 launch 次数。  
2. **半写共享表 + 假定已满** → 永不 Set **或** 半成品错数（同一地基）。  
3. **同组 flag 被两段 MIX 争用**（如 NTT∥INTT 同核）→ 错数或互锁。  
4. **深融合扩大握手面与半成品面**；拆 Host launch + mid-sync 用屏障换更短握手（代价：更多 sync）。  
5. **收窄握手是正确∧不卡的共同地基**，不是「只为不挂的额外手续」。

图谱：`J-HANG-UNREACHABLE-SET` → `J-HALF-TABLE-BLOCKS-SET` / `J-FLAG-CONTENTION` → `J-FUSION-EXPANDS-SURFACE`。

---

## 2. 证伪教训（勿重踩）

| 已证伪 | 留痕 |
|--------|------|
| 同核双 Cube ⇒ 充分挂因 | `J-DUAL-CUBE-NOT-SUFFICIENT` retracted |
| 多加 Host launch ⇒ 充分修复 | `J-EXTRA-LAUNCH-NOT-FIX` retracted（有效的是消断裂/争用，不是「加次数」） |

---

## 3. 设计权衡（默认怎么选）

| 决策 | 为何 | 代价 | 何时可破 |
|------|------|------|----------|
| 硬禁 flag5/7、Wait 内 SyncAll、SoftSync | 直接制造不可达 Set / 互锁 | 无 | 无 |
| 默认短 CrossCore：拆 launch + mid-sync | 融合扩大握手与半成品面 | 更多 Host sync | 握手已极短且有 NPU×N+性能数据 → **新决策**，禁默默融回 |
| flag∈{1,3,4} | 短表够用 | 表达力受限 | 单独论证 GATE |
| 共享表默认 `BLOCK_DIM=1` | 半写→永不 Set/错数 | 放弃双 AIV prep tick | 分片无交叠+Inline SHAKE 已证 |
| Decrypt：NTT∥INTT 分核 | flag 争用史 | 相对 fused +launch | 同短 CrossCore 破例条件 |
| 结构假说先 SIM；粘性认干净 NPU | SIM≠粘性 | 排程更重 | — |
| 不挂门禁 NPU×30/×100 | 单轮绿不够；排除杀挂污染 | 占卡 | — |
| 业务写出 UB+DataCopy；权威同 ISA | SetValue/异架构假绿 | — | — |
| NTT：poly-batch / limb 算术 / S1–S3 禁 Gather | 数学契约 | — | 换参数组须显式勾差 |

---

## 4. 假绿阶梯（`J-EVIDENCE-LADDER`）

| 绿了什么 | 能证明 | 不能证明 |
|----------|--------|----------|
| CPU 对拍 | 孪生路径 I/O 自洽 | NPU 字节正确（SetValue 假绿） |
| SIM | 缺 SET 等**结构**假说 | 粘性/污染挂 |
| NPU 单轮 | 该次能跑通 | 加压不挂 |
| NPU×N + liboqs | 双门禁（暂行） | Encrypt 粘性充分条件（`Q-ENCRYPT-STICKY` 仍开） |

---

## 5. 平台钉子（最小事实）

- CrossCore：`Wait` 须可达 `Set`；禁 flag **5/7**；AIC Wait 中禁 SyncAll；禁 SoftSync  
- `__aicore__` 字符串字面量是 `__gm__` → `constexpr uint8_t[]`  
- 杀挂后同卡连环挂 = **污染**，解释不了干净卡首次挂  

---

## 6. 附录（用法库存 · 不进推理主链）

### 6.1 stable ↔ 重建 Host launch 数（SIM/NPU；非 CPU 孪生）

| 算子 | stable | 重建 |
|------|--------|------|
| PKE KeyGen | 2 | 3 |
| PKE Encrypt | 2 | 2 |
| PKE Decrypt | 1 | 3 |
| KEM KeyGen | 2 | 4 |
| KEM Encaps | 2 | 2 |
| KEM Decaps | 3 | 5 |

口诀：Encaps/Encrypt=2；Decrypt=3；PKE-KG=3；KEM-KG=4；Decaps=5。  
次数相同 ≠ 拓扑同构。细节与刀号 → `graph-tests/*-rebuild-ops/`。

### 6.2 关闸指针

重建短 CrossCore 路径已观察到双门禁可过（Encaps/Decrypt/KeyGen）→ 图谱 `J-REBUILD-TOPO-PASSES-GATES`（薄）；**≠** 晋级 stable（须 `#交付#`）。

---

## 7. 刷新命令

```bash
python3 scripts/check_rg_dag.py --yaml docs/rg-ascendc-engineering.yaml
python3 scripts/rg_viz.py
```
