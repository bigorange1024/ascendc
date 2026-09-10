# 活跃用例 NPU 算子性能一览

| 项 | 内容 |
|----|------|
| **刷新** | 2026-09-10（六档 Task Duration；**D09** 补 `msopprof` 真实 cycle + Freq） |
| **平台** | Ascend910B3 真机；`ops-profiling`（Σ Task Duration）+ **`msopprof`**（`aic_total_cycles` / Freq；D09） |
| **口径** | 默认配置下各 kernel **Task Duration** 求和（µs）；有 msopprof 时另登 **Current/Rated Freq** 与 **aic/aiv_total_cycles**；未测为 `n/a` |
| **来源优先级** | `STATUS.md` 性能节 → 本表 → ops-profiling `docs/perf/round_*` / 远端 PROF |
| **范围** | 当前已上板登记的 `graph-tests/RB-*` 重建档；**不含** `frozen/`；**非** SIM Total tick（SIM 见 [`active_sim_regress_summary.md`](active_sim_regress_summary.md)，本表不改该文件） |
| **维护** | 手工登记（非 CI dump）；上板验收通过后刷新对应行 |
| **禁止** | 用 host `[wall_sec]` / 自造 wall 填本表 |

远端套件（本轮）：`/mnt/workspace/launch-reduce-logs/ops-profiling-rebuild-20260910-160409/`（`index.csv` 六档 `summary_rc=0`）。

---

## graph-tests 重建 — Launch 压缩终态（2026-09-10）

> 路径根：[`graph-tests/`](../graph-tests/)。Host launch 数见备注；Σ = 各 kernel Task Duration 之和。

| ID | 目录 | NPU Σ Task Duration (µs) | 备注 | 来源 |
|----|------|--------------------------|------|------|
| **K07** | [`RB-K07-pke-2launch`](../graph-tests/kg_related/RB-K07-pke-2launch/) | **1126.58** | Host **2**：prep **448.16** + `kg_ntt_dot_encode` **678.42**；cube_util 2.83% | STATUS / ops-profiling 2026-09-10 |
| **K09** | [`RB-K09-kem-2launch`](../graph-tests/kg_related/RB-K09-kem-2launch/) | **1227.12** | Host **2**：prep **447.16** + `kg_ntt_dot_encode` **779.96**；cube_util 2.47% | STATUS / ops-profiling 2026-09-10 |
| **T19** | [`RB-T19-encrypt-ek-decode-full`](../graph-tests/enc_related/RB-T19-encrypt-ek-decode-full/) | **1238.78** | Host **2**：prep **19.68** + compute **1219.10**；对照测（非 launch 压缩刀） | STATUS / ops-profiling 2026-09-10 |
| **T23** | [`RB-T23-encaps-liboqs-cross`](../graph-tests/enc_related/RB-T23-encaps-liboqs-cross/) | **1283.54** | Host **2**：prep **63.44** + compute **1220.10**；对照测 | STATUS / ops-profiling 2026-09-10 |
| **D09** | [`RB-D09-decrypt-1launch`](../graph-tests/dec_related/RB-D09-decrypt-1launch/) | **455.74** | Host **1**：`d09_decrypt_fused`；见下节 **cycle** | STATUS / ops-profiling + msopprof |
| **T30** | [`RB-T30-decaps-2launch`](../graph-tests/enc_related/RB-T30-decaps-2launch/) | **1719.22** | Host **2**：`t30_dec` **455.12** + `t30_enc` **1264.10** | STATUS / ops-profiling 2026-09-10 |

### 与 stable 同角色量级对照（参考，非门禁）

| 角色 | 重建（本表 Σ µs） | Host launch | stable 参考 launch |
|------|-------------------|-------------|-------------------|
| PKE KeyGen | K07 **1127** | 2 | 2 |
| KEM KeyGen | K09 **1227** | 2 | 2 |
| PKE Encrypt | T19 **1239** | 2 | 2 |
| KEM Encaps | T23 **1284** | 2 | 2 |
| PKE Decrypt | D09 **456** | 1 | 1 |
| KEM Decaps | T30 **1719** | 2 | 3 |

---

## D09 · msopprof 真实时钟周期（2026-09-10 17:22 · 910B3）

> 工具：`msopprof --aic-metrics=Default`（及 BasicInfo,PipeUtilization）；`docs/perf/round_002/`。  
> 远端：`/mnt/workspace/launch-reduce-logs/d09-msopprof-20260910-172204/`。  
> **Freq**：Current=**1800** / Rated=**1800**（满频）。  
> **换算校验**：`aic_total_cycles / 1.8e9 * 1e6 ≈ aic_time(us)`（cube0：722923 → 401.62 µs，一致）。

| 项 | 值 |
|----|-----|
| Task Duration | **454.58 µs**（msopprof；与 ops-profiling Σ **455.74** 同量级） |
| Block Dim / Mix | 1 / 2 |
| 头开销（Task − max core） | ~52.96 µs（~11.6%，msopprof 口径） |

| sub_block | time (µs) | **total_cycles** | 主占比 |
|-----------|-----------|------------------|--------|
| cube0 | 401.62 | **722923** | cube_ratio≈0.01%；scalar≈0.7%；icache_miss≈12.9% |
| vector0 | 451.87 | **813361** | **aiv_scalar_ratio≈96.9%**；vec≈0.008% |
| vector1 | 401.53 | **722750** | scalar 低；大量 `scalar_wait_id*`（等 CrossCore） |

**瓶颈结论（有 cycle 证据）**：主导核 vector0 几乎全程 scalar（~97% cycle），Cube 几乎空转 → **SCALAR BOUND / 握手等待**，非 MTE/Cube 算力墙。

### Timeline / JSON 图

| 尝试 | 结果 |
|------|------|
| CLI `PipeTimeline` | **本机 msopprof 无此指标** |
| `TimelineDetail` | 采集跑通但 **dump 解析失败**（`Failed to get any available dump file` / Msopt dump kernel failed）→ **无** instruction timeline JSON/HTML |
| `msopprof-visualization` 直读 OPPROF | 需 `collection_manifest.json`；未强行重采（Timeline 已红） |

下一刀若要 HTML Timeline：先查 dump 失败原因（或 `-g` Source 路径），再 `run_pipeline.py --feature instruction-timeline`。

---

## 采集命令（登记前）

```bash
# A) Σ Task Duration（六档套件）
bash thirdparty/cannbot-skills/ops/ops-profiling/scripts/msprof_profile_run.sh \
  --warm-up=3 --output=./msprof_output -- ./out/<bin>
python3 thirdparty/cannbot-skills/ops/ops-profiling/scripts/msprof_perf_summary.py \
  ./msprof_output/PROF_GROUP_* .

# B) 真实 cycle + Freq（单 kernel；输出目录须 chmod 700）
export LD_LIBRARY_PATH=$PWD/out/lib:$LD_LIBRARY_PATH
export ASCEND_DEVICE_ID=0
msopprof --output=$HOME/opprof_priv --warm-up=3 --launch-count=1 \
  --kernel-name=<kernel> --replay-mode=kernel \
  --aic-metrics=BasicInfo,PipeUtilization,Default \
  --application=./ascendc_kernels_bbit
python3 thirdparty/cannbot-skills/ops/ops-profiling/scripts/perf_summary.py \
  $HOME/opprof_priv/OPPROF_* .
```
