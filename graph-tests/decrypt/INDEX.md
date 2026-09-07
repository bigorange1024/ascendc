# graph-tests/decrypt — Decrypt fused 卡死 · 图谱驱动试验场

> **本目录作用**：Decrypt 线的「要做什么、图谱怎么管、实验怎么做」。  
> **不是** stable 交付树；**不是** 全量 Alg.15。toy 只隔离 **SoftSync / 双 GATE / 同核 NTT+INTT**。  
> **最后刷新**：2026-09-03

相关机读真理：[`docs/rg-decrypt-fused.yaml`](../../docs/rg-decrypt-fused.yaml)  
Encrypt 线（对照，不替代本目录）：[`docs/rg-encrypt-l18.yaml`](../../docs/rg-encrypt-l18.yaml) · [`../INDEX.md`](../INDEX.md)  
工具：[`thirdparty/reasoning-graph-skill/`](../../thirdparty/reasoning-graph-skill/)  
看图：[`docs/rg-decrypt-fused.html`](../../docs/rg-decrypt-fused.html)

**与 Encrypt 试验场的硬差**：Encrypt GT-1..7 **没有** `SoftSyncArrive`（AIV1 `while(s[slot]==0)`）。本线第一刀必须先隔离它。

---

## 1. 目标分层（勿混）

| 层级 | 目标 | 验收 |
|------|------|------|
| **最终（用户）** | NPU 上 `stable-fips203-mlkem-pke-decrypt-k4` **prod input-only** 不再 SynchronizeStream 卡死，且 `m` 对拍 | 图上 `Q-ULT` |
| **当前近目标** | 轻量 toy 隔离 Decrypt **独有**骨架 | `D-NEAR` |
| **toy 验收** | **不对算法正确性**；SIM 跑完或明确挂死（记末行） | `Q-TOY-SOFTSYNC` 等 |
| **本目录** | 验证图谱单点假设 | 各 `DGT-*` |

**验证口径**：过程只认 **SIM**；CPU 不进图。Cloud SIM 上 **stable 全链已绿未挂**（`F-SIM-STABLE-NO-HANG`）≠ 用户所述卡死已解。

**禁止**：把上机当第一实验；把 Encrypt toy 未挂当 Decrypt 答案；以改 stable 全链为主场（`D-NO-STABLE-AS-MAIN-LAB`）。

### 1.1 请用户上机前自检

未勾满 **不得** 请用户拷 stable decrypt 上实机：

1. SoftSync / GATE / 融合骨架假设在 SIM toy 上有节点与证据  
2. 失败对照已沉 `fail-lessons`  
3. 上机步骤 ≤ 半页（目录、env、期望 TRACE/softSync 日志）  
4. 改动集小  

---

## 2. 图谱

真理源：`docs/rg-decrypt-fused.yaml`。主控唯一可改 yaml；subagent 只读。

工作环：查图 → 挂假设 → 下发 toy → 收 SIM 反馈 → 刷新图谱 → `rg_validate` → 重渲 HTML。

### 2.1 已收入失败教训（勿再试）

| 节点 | 教训 |
|------|------|
| `J-FAIL-INTT-FLAGS-57` | INTT 禁用 flag 5/7 |
| `J-FAIL-SYNCALL-ON-AIC-WAIT` | AIC Wait 中禁 SyncAll |
| `J-FAIL-TPIPE-MARK-IN-FUSED` | fused 全链内 mark 新建 TPipe+DataCopy → 对拍坏 |
| `J-FAIL-TRACE-POLL-SIM-VERIFY` | `F203_DECRYPT_TRACE=1` 轮询不作 SIM golden |
| `J-FAIL-ENCRYPT-TOY-AS-DECRYPT-ANSWER` | Encrypt GT 未挂 ≠ Decrypt 已解 |

生产 SoftSync 是 **AIV0↔AIV1**（`F-SOFTSYNC-VS-ENCRYPT-FAIL`），不是 Encrypt 失败的 AIC-AIV SoftSync。本线 **要测** 生产定式，**不要** 再发明 AIC 参与的双向 SoftSync。

### 2.2 命令

```bash
python3 thirdparty/reasoning-graph-skill/scripts/rg_validate.py --yaml docs/rg-decrypt-fused.yaml
python3 thirdparty/reasoning-graph-skill/scripts/rg_query.py --yaml docs/rg-decrypt-fused.yaml --status open
python3 thirdparty/reasoning-graph-skill/scripts/rg_render.py \
  --yaml docs/rg-decrypt-fused.yaml \
  --out /opt/cursor/artifacts/rg-decrypt-fused.html
cp /opt/cursor/artifacts/rg-decrypt-fused.html docs/rg-decrypt-fused.html
```

---

## 3. 主控 vs Subagent

与 [`../INDEX.md`](../INDEX.md) §4 相同：同时 1 个 subagent；禁 push/开分支/改 yaml；下发必带时限字段。

---

## 4. 当前开放问题

| id | 问题 |
|----|------|
| `Q-ULT` | NPU prod input-only 是否不卡且正确？ |
| `Q-HANG-LOCI` | 若卡死：卡在哪一次 Wait/忙等？（toy 未复现） |

已答：`Q-TOY-SOFTSYNC` · `Q-TOY-SOFT-GATE` · `Q-TOY-FUSED-SKEL` · `Q-TOY-MULTI-LAUNCH`  
未证假设：`J-PRIMARY-SOFTSYNC`（单独忙等 SIM 未挂）· `J-TWO-GATE-DIFF`（双 GATE SIM 未挂，NPU 未知）

---

## 5. 建议刀序（2026-09-03）

1. **DGT-1（PASS）**：仅 SoftSyncArrive — SIM 未挂（tick 3979）  
2. **DGT-2（PASS）**：SoftSync + 一轮 GATE 4/8 — SIM 未挂（tick 5073）  
3. **DGT-3（PASS）**：融合骨架 — SIM 未挂（tick 13056）  
4. **DGT-4（PASS）**：同进程 16×launch — SIM 未挂（tick 181191）  
5. **下一刀（不催上机）**：更近生产体量的 toy，或 32×外层进程；**Q-ULT 仍 open**

---

## 6. 目录

| 路径 | 说明 |
|------|------|
| `INDEX.md` | 本文件 |
| `DGT-20260903-1.md` | 第一刀 SoftSync 隔离 — **PASS** |
| `DGT-20260903-2.md` | 第二刀 SoftSync+GATE — **PASS** |
| `DGT-20260903-3.md` | 第三刀融合骨架 — **PASS** |
| `DGT-20260903-4.md` | 第四刀多 launch — **PASS** |
| [`NPU-TRIP-20260904-decrypt.md`](NPU-TRIP-20260904-decrypt.md) | **实机测试（人话版）**：A toy→B Encrypt toy→C stable+TRACE |

增删时同步本表与根 [`../INDEX.md`](../INDEX.md)。
