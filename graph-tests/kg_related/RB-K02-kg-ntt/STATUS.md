# STATUS — RB-K02-kg-ntt

| 字段 | 值 |
|------|-----|
| 刀 | KGR-P02 · KeyGen L2a NTT(ŝ/ê) |
| 状态 | **PASS_CPU** |
| 日期 | 2026-09-09 |
| 墙钟 | ~17s（编译+CPU；kernel≈0.5s） |

## 目标达成

1. `KERNEL_TYPE_MIX_AIC_1_2`，`blockDim=1`；flag **1/3 + 4=GATE**
2. basename 唯一：`kg_ntt_custom.cpp`
3. I/O：时域 `ŝ[4,256]` / `ê[4,256]` int32 → NTT(ŝ) / NTT(ê)（int32）
4. AIV0：Alg.9 ForwardNTT ×8 poly；poly-batch 整 poly；禁 Gather/limbsplit
5. 写出：UB+DataCopy（X12）；不做点积/Encode/prep

## 验收证据

| 项 | 结果 |
|----|------|
| `bash run.sh -r cpu -v Ascend910B4` | SUCCESS；s_ntt/e_ntt **max=0**；PASS_SYNC+PASS_IO |
| Oracle | host FIPS Alg.9（`RB-T10/.../topology_math.mlkem_ntt`；ŝ/ê 自 `golden_se_sampling.build_src`；**非 liboqs**） |
| Seed | `SEED_D=20260619`（与 K01 对齐） |
| sync_audit | clean（无红线；SYNC-05 薄封装假阳性 + SYNC-09 性能） |
| SIM / NPU | 本刀未跑（TASK：subagent_cpu_sim） |

运营回报：[`../../keygen-rebuild-ops/tasks/KGR-P02-kg-ntt/FEEDBACK.md`](../../keygen-rebuild-ops/tasks/KGR-P02-kg-ntt/FEEDBACK.md)

## 布局速记

- 输入：`s.bin`/`e.bin` 行主序 `[4,256]`；`zetas[128]` Host 预喂
- 输出：`s_ntt.bin`/`e_ntt.bin`；ws 内 `OFF_S_NTT`/`OFF_E_NTT` 镜像
- 握手：AIV Set(4)→Set(1)→Wait(3)；AIC Wait(4)→Wait(1)→Cube→Set(3)
