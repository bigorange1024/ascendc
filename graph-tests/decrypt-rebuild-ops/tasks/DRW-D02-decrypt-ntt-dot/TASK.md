# DRW-D02 — Decrypt L2a NTT(u)+su_dot

| 字段 | 值 |
|------|-----|
| 状态 | **dispatched** |
| DAG | `E-D02-NTT-DOT` → `G-DG2-NTT-DOT` |
| 代码目录 | `graph-tests/dec_related/RB-D02-decrypt-ntt-dot/`（新建） |
| 运营目录 | `graph-tests/decrypt-rebuild-ops/tasks/DRW-D02-decrypt-ntt-dot/` |
| 墙钟 | ≤ 90 min |
| runner | **subagent**：编码 + **CPU only**；**禁止** `-r npu` / SSH |

继承 [`COMMON.md`](../../COMMON.md)。

## 目标

实现 S0A **L2a only**：

```text
u[k·256], ŝ[k·256]
  → û ← NTT(u)          // poly-batch；S1–S3 禁 Gather / limbsplit
  → ŵ ← Σ_j MultiplyNTTs(ŝ[j], û[j])   // 单 poly 输出
```

- 核：**MIX**（AIC+AIV）；flag ∈ **{1,3}**，可选 **4=GATE**；禁 5/7 / SoftSync。  
- basename：**`dec_ntt_dot_custom.cpp`**（全局唯一）。  
- `BLOCK_DIM=1`。  
- 写出 `û`/`ŵ`：UB+DataCopy（X12）。  
- 输入：可由本刀 `gen_data` 造合法 `u/ŝ`（host NTT/dot oracle），或复用 D01 同 seed 的 `output` 契约；**勿** `#include` D01 源码当库——可复制 **bin 布局约定**。

## 非目标

- 不写 prep / INTT / extract / Decaps。  
- 不跑 NPU；SIM 非门禁（TASK 不要求）。  
- 不把 NTT+INTT 融进本核（X15）。

## 必读

1. KB §B2 拓扑、§A3 NTT、§C X1/X12/X14/X15  
2. DAG：`F-DEC-TOPO`、`E-D02-NTT-DOT`、`C-SYNC-AUDIT`  
3. S0A FEEDBACK Launch L2a 行  
4. `docs/notes/MLKEM-NTT-实现总结.md`、`MLKEM-NTT-向量与标量实现指南.md`、`F203-innerproduct-k4-技术总结.md`  
5. cannbot：`api-crosscore-sync.md` + **本刀必须** `sync_audit.py`  
6. 可参考 **契约/壳**：`graph-tests/toys/RB-T01-mix-ntt13-handshake`、`enc_related/RB-T12-device-ntt-y` 的 LAYOUT/STATUS——**禁大段照抄**进 Decrypt；禁抄 alg15 / T25  
7. D01 STATUS（I/O 尺寸）：`dec_related/RB-D01-decrypt-prep/STATUS.md`

## 禁令

见 COMMON。另：禁 prep∥NTT 同 launch；禁与 `dec_prep_custom.cpp` 撞名。

## 验收（Subagent）

```bash
cd graph-tests/dec_related/RB-D02-decrypt-ntt-dot
bash run.sh -r cpu -v Ascend910B4
python3 thirdparty/cannbot-skills/ops/ascendc-sync-audit/scripts/sync_audit.py \
  --check all --format json . \
  > ../../decrypt-rebuild-ops/tasks/DRW-D02-decrypt-ntt-dot/logs/sync_audit.json
```

| 项 | 判据 |
|----|------|
| CPU | `û`/`ŵ` vs host/NTT+dot oracle max=0（可非 liboqs；FEEDBACK 标明） |
| CrossCore | Set/Wait 成对；flag∈{1,3,4}；AIC Wait 环无 SyncAll |
| sync_audit | 无红线（或人工确认假阳性写入 FEEDBACK） |
| basename | `dec_ntt_dot_custom.cpp` |

## 回报

- `FEEDBACK.md` + `logs/sync_audit.json` + 实现 `STATUS.md`  
- 更新 `dec_related/INDEX.md` D02 行  
- 禁改 KB/DAG

## next_hint

D02 CPU 绿 → 主控派 **DRW-D03**（`dec_intt_extract_custom`）。
