# EP06-encaps-2launch · STATUS

> DAG：本战役 `encrypt-encaps-low-launch` · C2  
> 日期：2026-09-12  
> 结论：**PASS**（CPU + `SIM_DIRECT=1` sim；`c`/`K` vs liboqs Encaps **max=0**；Host launch=**2**）

## 目标

将 EP04 的多段 Encaps×liboqs 接线，收敛为 **prep + compute = 2 Host launch**，且 `c`/`K` 仍 ≡ liboqs。

## Host 外形

| 段 | 内容 |
|----|------|
| Host H/G | Encaps 头：H/G→K（无 launch） |
| L1 | `enc_prep_l1`：SampleNTT + CBD |
| mid | Â→Âᵀ；上传 t̂/e/μ/矩阵（无 launch） |
| L2 | `enc_compute_l2`：NTT→matvec→dot→INTT×2→加噪→pack→c |

## 验收

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

| 模式 | exit | wall | tick | host_launch | c/K max |
|------|------|------|------|-------------|---------|
| CPU | 0 | ~2.2s | — | 2 | **0** |
| SIM | 0 | ~138s | ~898662 | 2 | **0** |

日志：`/opt/cursor/artifacts/low-launch-sim/ep06-cpu.log`、`ep06-sim.log`。

## 禁令

- 本战役禁 `-r npu`  
- 不以 EP04 多 launch 接线版宣称交付形态  
- 未抄 `frozen/` / `enc_related/RB-T*` 核源码
