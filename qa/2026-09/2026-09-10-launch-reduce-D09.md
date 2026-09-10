# 2026-09-10 · Launch 压缩 · Decrypt→1（RB-D09）· Decaps→2（RB-T30）

## 决策

- 用户要求：**重写实验代码，勿改已有树** → 新建 `RB-D09-decrypt-1launch`，不动 D08/T28/T29。
- Compress₁ 对齐 liboqs 常数路径（非学校式 (2x+q/2)/q）。
- SyncAll 仅在 CrossCore Wait 环外；flag∈{1,3}。

## 结果

- sync_audit：无红线（SYNC-05 遗留同族）。
- NPU 冒烟 PASS；NPU×30 **ok=30 fail=0**（每轮换 SEED_D）。
- QUEUE 1–7 全绿；Decrypt launch 数对齐 stable=1。

## 遗留

- Wave4 经验入库（KB 图改须用户授权）。

---

## T30 · Decaps → 2 Host launch（独立新树）

### 决策

- 新建 `graph-tests/enc_related/RB-T30-decaps-2launch/`；**禁止**改 T28/T29/D08/D09；**禁止**抄核源码。
- Host：**L1** `t30_dec_fused_custom`（Decrypt 全链）+ **L2** `t30_enc_fused_custom`（prep G/CBD/Â…→SyncAll→NTT 路径）+ Host FO。
- magic `0x54333032`；namespace `t30_dec`/`t30_enc`/`rb_t30`；Compress₁ C=41285357。

### 结果

- Cloud：CPU **PASS**；SIM_DIRECT **PASS**。
- **NPU 冒烟 PASS**（≈2.25s）；**NPU×30 ok=30 fail=0**。
- QUEUE 1–8 全绿；Decaps=**2**（优于 stable=3）。
- 证据：`t30-smoke-20260910-153112.log` · `t30-x30-20260910-153535.log`。
- keepalive 已停；板可关。

---

## 性能收口（对齐教材三层口径 · ops-profiling 采设备真值）

### 决策

- 工程填表权威：[`docs/research/教材KEM实机测量清单.md`](../../docs/research/教材KEM实机测量清单.md) — **1 设备 Task Duration → 2 Host launch → 3 wall**；禁止 host wall 当结论。
- 本轮重建树用 cannbot **`ops/ops-profiling`**（`msprof_profile_run.sh` + `msprof_perf_summary.py`）采层 1；数字落盘 [`PERF.md`](../../graph-tests/launch-reduce-ops/PERF.md) + 各用例 `STATUS.md`。
- 规则写入 [`COMMON.md`](../../graph-tests/launch-reduce-ops/COMMON.md)。

### 结果（910B3 · 六档 `summary_rc=0`）

| 算子 | launches | Σ Task Duration |
|------|----------|-----------------|
| PKE KeyGen K07 | 2 | 1126.58 µs |
| KEM KeyGen K09 | 2 | 1227.12 µs |
| PKE Encrypt T19 | 2 | 1238.78 µs |
| KEM Encaps T23 | 2 | 1283.54 µs |
| PKE Decrypt D09 | 1 | 455.74 µs |
| KEM Decaps T30 | 2 | 1719.22 µs |

- 套件：`…/ops-profiling-rebuild-20260910-160409/`；共性：cube_util≈2–4%、AIV scalar≈50–65%。
- keepalive 再确认已停。
