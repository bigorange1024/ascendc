# DRW-D03 — Decrypt L2b INTT+extract→m

| 字段 | 值 |
|------|-----|
| 状态 | **dispatched** |
| DAG | `E-D03-INTT-M` → `G-DG3-INTT-M` |
| 代码目录 | `graph-tests/dec_related/RB-D03-decrypt-intt-extract/`（新建） |
| 运营目录 | `graph-tests/decrypt-rebuild-ops/tasks/DRW-D03-decrypt-intt-extract/` |
| 墙钟 | ≤ 90 min |
| runner | **subagent**：编码 + **CPU only**；**禁止** `-r npu` / SSH |

继承 [`COMMON.md`](../../COMMON.md)。

## 目标

实现 S0A **L2b only**：

```text
ŵ[256]（NTT 域内积结果；可自 gen_data / D02 同契约）
v[256]（密文解压后）
  → w ← INTT(ŵ)     // 可含 pad 约定；与 D02 输出布局对齐
  → m[32] ← extract / Compress₁ 逆路径（与 v 组合，按 FIPS Alg.15 尾段）
```

- 核：**MIX**；flag ∈ **{1,3}**，可选 **4=GATE**（与 L2a **分核复用号**，本刀独立 launch，勿与 D02 同 binary 除非 basename 皆唯一）。  
- basename：**`dec_intt_extract_custom.cpp`**。  
- `BLOCK_DIM=1`；写出 `m`：**UB+DataCopy**（X12）。  
- 输入布局须与 D02 `ŵ` / D01 `v` 契约一致（尺寸见 D01/D02 STATUS）。

## 非目标

- 不写 prep / NTT+dot；不把 NTT∥INTT 融进单核（X15）。  
- 不跑 NPU；不要求 SIM。  
- 不在本刀强求 liboqs（可用 host FIPS oracle；FEEDBACK 标明）。全链 liboqs 留给拼装刀 / 主控 NPU。

## 必读

1. KB §B2、§C X12/X15；DAG `E-D03-INTT-M`  
2. S0A FEEDBACK L2b 行  
3. NTT/INTT 定稿笔记；Compress/Decompress 笔记（extract 尾）  
4. D02/D01 STATUS（ŵ/v/m 尺寸）  
5. cannbot crosscore + **必跑 sync_audit**  
6. 可参考 toys/INTT 契约；**禁抄** alg15 / T25 / decrypt 算子树源码

## 禁令

COMMON 全条；禁与 `dec_prep_custom` / `dec_ntt_dot_custom` 撞名；禁 SoftSync/5·7。

## 验收（Subagent）

```bash
cd graph-tests/dec_related/RB-D03-decrypt-intt-extract
bash run.sh -r cpu -v Ascend910B4
# sync_audit → 本刀 logs/
```

| 项 | 判据 |
|----|------|
| CPU | `m[32]` vs host/FIPS（或可得的 liboqs）max=0 |
| CrossCore | flag∈{1,3,4}；配对；Wait 环无 SyncAll |
| X12 | `m` 经 DataCopy |
| basename | `dec_intt_extract_custom.cpp` |

## 回报

FEEDBACK + STATUS + 更新 `dec_related/INDEX.md`；禁改 KB/DAG。

## next_hint

D03 绿 → 主控开 **拼装全链 Decrypt**（三 launch + mid-sync）或先标 DG3 关；然后请用户开机做 NPU/liboqs。
