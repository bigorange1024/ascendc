# 重建新算子 910B3 性能（ops-profiling）

> **日期**：2026-09-10  
> **技能**：[`thirdparty/cannbot-skills/ops/ops-profiling`](../../thirdparty/cannbot-skills/ops/ops-profiling/SKILL.md)  
> **采集**：`msprof_profile_run.sh --warm-up=3`  
> **解析**：`msprof_perf_summary.py`  
> **口径**：设备侧 PipeUtilization `op_summary` **Task Duration**（**禁止** host wall）

## Σ Task Duration

| 算子 | launches | Σ Task Duration (µs) | 分 kernel |
|------|----------|----------------------|-----------|
| PKE KeyGen K07 | 2 | 1126.58 | prep 448.16 + ntt_dot_encode 678.42 |
| KEM KeyGen K09 | 2 | 1227.12 | prep 447.16 + ntt_dot_encode 779.96 |
| PKE Encrypt T19 | 2 | 1238.78 | prep 19.68 + compute 1219.10 |
| KEM Encaps T23 | 2 | 1283.54 | prep 63.44 + compute 1220.10 |
| PKE Decrypt D09 | 1 | 455.74 | d09_decrypt_fused |
| KEM Decaps T30 | 2 | 1719.22 | t30_dec 455.12 + t30_enc 1264.10 |

远端全量：`/mnt/workspace/launch-reduce-logs/ops-profiling-rebuild-20260910-160409/`（`SUMMARY.md` / `*.summary.txt` / 各用例 `docs/perf/round_001/`）。
