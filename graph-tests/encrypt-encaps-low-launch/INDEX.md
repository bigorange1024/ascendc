# Encrypt / Encaps · 少 Host launch（SIM 全实验）

> **锁定（2026-09-12 夜 · 用户）**  
> - **cann-ntt 版** Encrypt / Encaps：**Host launch = 2**  
> - **AIV NTT 版** Encrypt / Encaps：**Host launch = 1**  
> - **只做 SIM 完整实验**（`cpu` + `sim`）；本战役不占 NPU  
> - Agent **自主推进**，不打断用户

## 目标

| 路线 | Host launch | 入口资产 | 出口 |
|------|-------------|----------|------|
| AIV | **1** | `../aiv-kem-vector-sim/` AE-E / AE-P | cpu+SIM，c(/K)≡liboqs；审计 launch=1 |
| cann-ntt | **2** | 基于 `enc_cann_ntt` 积木，**新**少 launch 用例 | 从 8 launch 融成 prep+compute；cpu+SIM≡liboqs |

**否决**：继续以 8×`ACLRT_LAUNCH_KERNEL` 接线版当作交付/正确形态。

## 文档

| 文件 | 用途 |
|------|------|
| [PLAN.md](PLAN.md) | 波次 |
| [QUEUE.md](QUEUE.md) | 执行队列（**本战役 DONE**） |

## 出口（2026-09-12 夜 · SIM 全实验）

| 用例 | Host launch | cpu | sim | liboqs |
|------|-------------|-----|-----|--------|
| [`../aiv-kem-vector-sim/AE-E-encrypt`](../aiv-kem-vector-sim/AE-E-encrypt/) | **1** | PASS | PASS | c max=0 |
| [`../aiv-kem-vector-sim/AE-P-encaps`](../aiv-kem-vector-sim/AE-P-encaps/) | **1** | PASS | PASS | c/K max=0 |
| [`../enc_cann_ntt/EN15-encrypt-2launch`](../enc_cann_ntt/EN15-encrypt-2launch/) | **2** | PASS | PASS | c max=0 |
| [`../enc_cann_ntt/EP06-encaps-2launch`](../enc_cann_ntt/EP06-encaps-2launch/) | **2** | PASS | PASS | c/K max=0 |

证据：`/opt/cursor/artifacts/low-launch-sim/`（`SUMMARY.txt` · `PASS-greps.txt` · `launch-audit.txt` · 各 `*-cpu/sim.log`）。

## 相关

- AIV 资产：[`../aiv-kem-vector-sim/`](../aiv-kem-vector-sim/INDEX.md)  
- cann-ntt 积木（只读参照）：[`../enc_cann_ntt/`](../enc_cann_ntt/INDEX.md) · [`../enc-encaps-cann-ntt-sim/`](../enc-encaps-cann-ntt-sim/INDEX.md)  
- 历史 2-launch 外形：`enc_related` T19 等（对照，不抄 frozen）
