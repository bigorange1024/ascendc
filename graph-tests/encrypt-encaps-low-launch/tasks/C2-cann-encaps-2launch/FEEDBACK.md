# FEEDBACK · C2 cann-ntt Encaps 2-launch（EP06）

## 结论

**PASS**（cpu + `SIM_DIRECT=1` sim）

## 目录

`graph-tests/enc_cann_ntt/EP06-encaps-2launch/`

## Launch 审计

Host `ACLRT_LAUNCH_KERNEL`：

1. `enc_prep_l1`  
2. `enc_compute_l2`  

Host 侧 H/G→K 与 mid（Â→Âᵀ 等）**无** launch。  
运行时：`host_launch_count=2 (expect 2)`。

## 验收

| 模式 | exit | c / K vs liboqs | wall / tick | 日志 |
|------|------|-----------------|-------------|------|
| cpu | 0 | max=0 / max=0 | ~2.2s | `ep06-cpu.log` |
| sim | 0 | max=0 / max=0 | wall≈138s；tick≈898662 | `ep06-sim.log` |

## 门禁

- Host launch = **2**  
- 禁 `-r npu`；stray dump 已收拢 `sim_log/`  
- 硬门禁：`liboqs_kem_vs` c/K **HARD PASS**
