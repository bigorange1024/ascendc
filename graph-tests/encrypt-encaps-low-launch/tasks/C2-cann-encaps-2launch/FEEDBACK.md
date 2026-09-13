# FEEDBACK · C2 cann-ntt Encaps 2-launch（EP06）

## 结论

**PASS**（cpu + `SIM_DIRECT=1` sim）

## 目录

`graph-tests/enc_cann_ntt/EP06-encaps-2launch/`

## Launch 审计

Host `ACLRT_LAUNCH_KERNEL`：

1. `enc_prep_l1`  
2. `enc_compute_l2`  

Host 侧 H/G→K 与 mid（**无 Â 转置**，仅上传 L2 输入）**无** launch。  
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

> **2026-09-13 纠偏**：初版「Host mid Â→Âᵀ」为错误捷径，已改设备侧换下标；不得再沉淀为推荐。

> **2026-09-13 I/O 整改**：禁止 Host 喂 e1/e2/μ 及任何中间态 .bin（LUT 除外）；开场一次 H2D；设备 CBD→y/e1/e2，μ←m。SIM 已对拍 liboqs max=0。
