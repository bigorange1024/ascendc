# 重建算子实机性能记录（2026-09-10 · Ascend910B3）

> **对齐工程口径**：[`docs/research/教材KEM实机测量清单.md`](../../docs/research/教材KEM实机测量清单.md)「多 launch 测准」三层表。  
> **填表优先填本页「设备真值」行**；禁止把 host `[wall_sec]` / 自造 wall 当结论。

---

## 口径（与教材清单同优先级）

| 优先级 | 本轮来源 | 含义 |
|--------|----------|------|
| **1 设备真值** | cannbot **`ops-profiling`**：`msprof_profile_run.sh --warm-up=3` → PipeUtilization `op_summary_*.csv` 各 kernel **Task Duration**；`msprof_perf_summary.py` 归档 `docs/perf/round_001/` | 与教材 `[msprof_kernel]` / `[msprof_kernel_total]`（`kernel_details` 求和）**同属设备侧 Task Duration**；多 launch = 各 kernel 行相加 |
| **2 Host 逐 launch** | 本轮**未系统采**（graph-tests 重建树尚未统一挂 `[npu_launch]` JSONL） | 对照用；有则填，无则留空 |
| **3 进程墙钟** | `[wall_sec]` | **不作填表真值**（含 host/ACL/D2H） |

| 项 | 值 |
|----|-----|
| 机 | CANNLab · Ascend910B3 · `ASCEND_DEVICE_ID=0` |
| 技能 | `thirdparty/cannbot-skills/ops/ops-profiling` |
| 套件日志 | `/mnt/workspace/launch-reduce-logs/ops-profiling-rebuild-20260910-160409/`（`index.csv` 六档 `summary_rc=0`） |
| 归档 | 各用例 `docs/perf/round_001/summary.txt` + `msprof_output/PROF_GROUP_*`（远端工作树） |

**与教材脚本关系**：stable/examples KEM 默认走 `MSPROF_MODE=app` + [`scripts/npu_msprof_summarize.py`](../../scripts/npu_msprof_summarize.py)。本轮重建树用 ops-profiling 做**标准 7 组 aic-metrics + 摘要**；填「整算子耗时」时仍报 **Σ Task Duration**，与教材「优先填设备真值」一致。

---

## 表 — 设备真值（Σ Task Duration）

| # | 算子 | 路径 | Host launches | Σ Task Duration (µs) | Σ (ms) |
|---|------|------|---------------|----------------------|--------|
| 1 | PKE KeyGen | `kg_related/RB-K07-pke-2launch` | 2 | **1126.58** | 1.127 |
| 2 | KEM KeyGen | `kg_related/RB-K09-kem-2launch` | 2 | **1227.12** | 1.227 |
| 3 | PKE Encrypt | `enc_related/RB-T19-encrypt-ek-decode-full` | 2 | **1238.78** | 1.239 |
| 4 | KEM Encaps | `enc_related/RB-T23-encaps-liboqs-cross` | 2 | **1283.54** | 1.284 |
| 5 | PKE Decrypt | `dec_related/RB-D09-decrypt-1launch` | 1 | **455.74** | 0.456 |
| 6 | KEM Decaps | `enc_related/RB-T30-decaps-2launch` | 2 | **1719.22** | 1.719 |

---

## 分 kernel（对应教材 `[msprof_kernel]` 行）

| 算子 | kernel | TaskType | Task Duration (µs) |
|------|--------|----------|-------------------|
| K07 | `kg_prep_custom` | AI_VECTOR_CORE | 448.16 |
| K07 | `kg_ntt_dot_encode_custom` | MIX_AIC | 678.42 |
| K09 | `kg_prep_custom` | AI_VECTOR_CORE | 447.16 |
| K09 | `kg_ntt_dot_encode_custom` | MIX_AIC | 779.96 |
| T19 | `prep_custom` | AI_VECTOR_CORE | 19.68 |
| T19 | `compute_custom` | MIX_AIC | 1219.10 |
| T23 | `prep_custom` | AI_VECTOR_CORE | 63.44 |
| T23 | `compute_custom` | MIX_AIC | 1220.10 |
| D09 | `d09_decrypt_fused_custom` | MIX_AIC | 455.74 |
| T30 | `t30_dec_fused_custom` | MIX_AIC | 455.12 |
| T30 | `t30_enc_fused_custom` | MIX_AIC | 1264.10 |

---

## 主 MIX 摘要（ops-profiling `summary.txt` 摘录）

填瓶颈/调优用；**不替代**上表 Σ。

| 算子 | 主 Op | Task Duration | cube_util | AIV scalar | 头开销 |
|------|-------|---------------|-----------|------------|--------|
| K07 | `kg_ntt_dot_encode_custom` | 678.42 µs | 2.83% | 61.5% | 21.7% |
| K09 | `kg_ntt_dot_encode_custom` | 779.96 µs | 2.47% | 64.7% | 25.3% |
| T19 | `compute_custom` | 1219.1 µs | 3.81% | 55.1% | 11.9% |
| T23 | `compute_custom` | 1220.1 µs | 3.80% | 55.0% | 12.0% |
| D09 | `d09_decrypt_fused_custom` | 455.74 µs | 4.44% | 51.8% | 5.6% |
| T30 | `t30_enc_fused_custom` | 1264.1 µs | 3.85% | 54.8% | 11.5% |

深挖走 `ascend-profiling-anomaly` / cannbot 性能优化 skill；勿自造指标。

---

## 复现命令（本轮）

```bash
# 用例目录内（远端已装 cannbot ops-profiling）
bash thirdparty/cannbot-skills/ops/ops-profiling/scripts/msprof_profile_run.sh \
  --warm-up=3 --output=./msprof_output -- ./out/ascendc_kernels_bbit   # 以该用例实际 bin 名为准
python3 thirdparty/cannbot-skills/ops/ops-profiling/scripts/msprof_perf_summary.py \
  ./msprof_output/PROF_GROUP_* .
```

教材 KEM 路径仍推荐：

```bash
RUN_WITH_MSPROF=1 MSPROF_MODE=app bash run.sh -r npu -v Ascend910B4
python3 scripts/npu_msprof_summarize.py .
# 填表：优先 [msprof_kernel_total] / [msprof_kernel]
```
