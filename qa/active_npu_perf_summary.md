# 活跃用例 NPU 算子性能一览

| 项 | 内容 |
|----|------|
| **刷新** | 2026-09-11（六档 Task Duration 仍用 09-10；**PKE Decrypt 单 launch 融合核** profiling 深采收口） |
| **平台** | Ascend910B3 真机；`ops-profiling`（Σ Task Duration）+ **`msopprof`**（真实 cycle / Freq）+ **`msprof --export`**（应用级 Chrome Trace JSON） |
| **口径** | 默认配置下各 kernel **Task Duration** 求和（µs）；有 msopprof 时另登 **Current/Rated Freq** 与 **aic/aiv_total_cycles**；未测为 `n/a` |
| **来源优先级** | `STATUS.md` 性能节 → 本表 → ops-profiling `docs/perf/round_*` / 远端 PROF |
| **范围** | 当前已上板登记的 `graph-tests/RB-*` 重建档；**不含** `frozen/`；**非** SIM Total tick（SIM 见 [`active_sim_regress_summary.md`](active_sim_regress_summary.md)，本表不改该文件） |
| **维护** | 手工登记（非 CI dump）；上板验收通过后刷新对应行 |
| **禁止** | 用 host `[wall_sec]` / 自造 wall 填本表 |

远端套件（本轮）：`/mnt/workspace/launch-reduce-logs/ops-profiling-rebuild-20260910-160409/`（`index.csv` 六档 `summary_rc=0`）。

---

## graph-tests 重建 — Launch 压缩终态（2026-09-10）

> 路径根：[`graph-tests/`](../graph-tests/)。Host launch 数见备注；Σ = 各 kernel Task Duration 之和。

| 实验（角色） | 目录 | NPU Σ Task Duration (µs) | 备注 | 来源 |
|----|------|--------------------------|------|------|
| **PKE KeyGen · 2 launch** | [`RB-K07-pke-2launch`](../graph-tests/kg_related/RB-K07-pke-2launch/) | **1126.58** | Host **2**：prep **448.16** + `kg_ntt_dot_encode` **678.42**；cube_util 2.83% | STATUS / ops-profiling 2026-09-10 |
| **KEM KeyGen · 2 launch** | [`RB-K09-kem-2launch`](../graph-tests/kg_related/RB-K09-kem-2launch/) | **1227.12** | Host **2**：prep **447.16** + `kg_ntt_dot_encode` **779.96**；cube_util 2.47% | STATUS / ops-profiling 2026-09-10 |
| **PKE Encrypt · 全路径对照** | [`RB-T19-encrypt-ek-decode-full`](../graph-tests/enc_related/RB-T19-encrypt-ek-decode-full/) | **1238.78** | Host **2**：prep **19.68** + compute **1219.10**；对照测（非 launch 压缩刀） | STATUS / ops-profiling 2026-09-10 |
| **KEM Encaps · liboqs 交叉对照** | [`RB-T23-encaps-liboqs-cross`](../graph-tests/enc_related/RB-T23-encaps-liboqs-cross/) | **1283.54** | Host **2**：prep **63.44** + compute **1220.10**；对照测 | STATUS / ops-profiling 2026-09-10 |
| **PKE Decrypt · 单 launch 融合核** | [`RB-D09-decrypt-1launch`](../graph-tests/dec_related/RB-D09-decrypt-1launch/) | **455.74** | Host **1**：融合 decrypt；见下节 **cycle + profiling 收口** | STATUS / ops-profiling + msopprof |
| **KEM Decaps · 2 launch** | [`RB-T30-decaps-2launch`](../graph-tests/enc_related/RB-T30-decaps-2launch/) | **1719.22** | Host **2**：dec **455.12** + enc **1264.10** | STATUS / ops-profiling 2026-09-10 |

### 与 stable 同角色量级对照（参考，非门禁）

| 角色 | 重建（本表 Σ µs） | Host launch | stable 参考 launch |
|------|-------------------|-------------|-------------------|
| PKE KeyGen | **1127** | 2 | 2 |
| KEM KeyGen | **1227** | 2 | 2 |
| PKE Encrypt | **1239** | 2 | 2 |
| KEM Encaps | **1284** | 2 | 2 |
| PKE Decrypt | **456** | 1 | 1 |
| KEM Decaps | **1719** | 2 | 3 |

---

## PKE Decrypt · 单 Host launch 融合核 — profiling 收口（2026-09-11）

> 实验目录：[`graph-tests/dec_related/RB-D09-decrypt-1launch`](../graph-tests/dec_related/RB-D09-decrypt-1launch/)。  
> 可读产物：[`docs/perf/round_003_pke_decrypt_1launch/`](../docs/perf/round_003_pke_decrypt_1launch/)（含 `README.md`、`report.html`、`msprof_*.json`）。  
> 远端全集：`/mnt/workspace/launch-reduce-logs/pke-decrypt-1launch-prof-20260911-104959/`。  
> **Freq**：Current=**1800** / Rated=**1800**（满频）。

### 真实 cycle（msopprof PipeUtilization，复测与 09-10 一致）

| 项 | 值 |
|----|-----|
| Task Duration | **454.3 µs**（msopprof；与 ops-profiling Σ **455.74** 同量级） |
| Block Dim / Mix | 1 / 2 |

| 子核 | time (µs) | **total_cycles** | 主占比 |
|------|-----------|------------------|--------|
| cube0 | 401.61 | **722907** | cube≈0.01%；大量时间在 `scalar_wait_id1`（等跨核） |
| vector0 | 451.79 | **813224** | **scalar≈96.8%**；vec≈0.008% |
| vector1 | 402.14 | **723855** | 自身 scalar 低；大量 `scalar_wait_id*` 等 CrossCore |

**瓶颈（有 cycle 证据）**：vector0 几乎全程 scalar → **scalar / 握手等待主导**，不是 Cube/MTE 算力墙。

### 图 / JSON / HTML（本轮收口结果）

| 采集内容 | 结果 | 产物 |
|----------|------|------|
| 应用级 Chrome Trace（`msprof` 采集 + `--export`） | **成功** | `msprof_20260911105015.json`（74 事件；Host/Device 任务级，可 `chrome://tracing`） |
| cannbot 可视化 HTML（details/roofline/cache/raw-data） | **成功** | `report.html` + `collection_manifest.json` |
| 核内指令流水 `TimelineDetail` | **失败（复现）** | dump kernel args 失败 → 无指令级 JSON/HTML |
| CLI `PipeTimeline` | **本机无此指标** | HTML timeline 页按策略省略 |

**profiling 工作对该实验视为收口**：能采的 cycle / 应用级 JSON / HTML 报告已落盘；核内指令时间轴受工具链限制，不是漏跑。

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
