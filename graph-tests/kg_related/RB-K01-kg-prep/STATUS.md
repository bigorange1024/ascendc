# STATUS — RB-K01-kg-prep

| 字段 | 值 |
|------|-----|
| 刀 | KGR-P01 · KeyGen L1 prep |
| 状态 | **PASS_CPU** |
| 日期 | 2026-09-09 |
| 墙钟 | ~21s（编译+CPU；kernel≈4.3s） |

## 目标达成

1. `KERNEL_TYPE_AIV_ONLY`，`blockDim=1`；无 MIX / CrossCore
2. basename 唯一：`kg_prep_custom.cpp`
3. I/O：`seed_d[4B]` → `Â[16,256]` / `ŝ[4,256]` / `ê[4,256]`（另 dump 扁平 `src[8,256]` + `prf_out[8,128]`）
4. 积木接线（只 `-I`，不抄进本目录）：lines3-7 `BuildAHat16ShardFromSeedD` + lines8-15 `BuildSrcFromSeedD`（V3 + alg8 CBD）
5. Seed：`SEED_D=20260619` → Derand `exp-mlkem-f203-2s1e-k4:SEED_D=` → G(d‖k) → ρ/σ；PRF=SHAKE256

## 验收证据

| 项 | 结果 |
|----|------|
| `bash run.sh -r cpu -v Ascend910B4` | SUCCESS；Â/ŝ/ê/src/prf **max=0** |
| Oracle | host Python FIPS 契约（alg7_geom + `golden_se_sampling`，**非 liboqs**） |
| sync_audit | clean（无红线；仅 SYNC-09 性能提示） |
| SIM / NPU | 本刀未跑（TASK：subagent_cpu_sim） |

运营回报：[`../../keygen-rebuild-ops/tasks/KGR-P01-kg-prep/FEEDBACK.md`](../../keygen-rebuild-ops/tasks/KGR-P01-kg-prep/FEEDBACK.md)

## 布局速记

- `a_hat` 行主序：`offset=(p*4+j)*256`
- `src` 扁平等价：行 0..3=ŝ，4..7=ê；业务输出拆为 `s_hat.bin` / `e.bin`（UB+DataCopy）
