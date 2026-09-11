# STATUS — RB-K03-kg-dot-encode

| 字段 | 值 |
|------|-----|
| 刀 | KGR-P03 · KeyGen L2b Â∘ŝ̂+ê̂ + ByteEncode₁₂ |
| 状态 | **PASS_CPU** |
| 日期 | 2026-09-09 |
| 墙钟 | ~17s（编译+CPU；kernel≈0.58s） |

## 目标达成

1. `KERNEL_TYPE_MIX_AIC_1_2`，`blockDim=1`；flag **1/3 + 4=GATE**
2. basename 唯一：`kg_dot_encode_custom.cpp`
3. I/O：Â[16,256] / ŝ̂/ê̂[4,256] / γ / ρ → `ek_pke[1568]` / `dk_pke[1536]`（另 dump `t_hat`）
4. AIV0：Alg.11 4×4 内积 + ê̂ + Alg.5 BE₁₂ + 拼 ek‖ρ / dk；写出 UB+DataCopy（X12）
5. 不做 prep / NTT；禁抄 KeyGen / Encaps / Decrypt 整核

## 验收证据

| 项 | 结果 |
|----|------|
| `bash run.sh -r cpu -v Ascend910B4` | SUCCESS；ek/dk/t_hat **max=0**；PASS_SYNC+PASS_IO |
| Oracle | host FIPS（SampleNTT + `mlkem_ntt` + `multiply_ntts` + BE₁₂；**非 liboqs**） |
| Seed | `SEED_D=20260619`（与 K01/K02 对齐） |
| sync_audit | clean（无红线；SYNC-05 薄封装假阳性 + SYNC-09 性能） |
| SIM / NPU | 本刀未跑（TASK：subagent_cpu_sim） |

运营回报：[`../../keygen-rebuild-ops/tasks/KGR-P03-kg-dot-encode/FEEDBACK.md`](../../keygen-rebuild-ops/tasks/KGR-P03-kg-dot-encode/FEEDBACK.md)

## 布局速记

- Â 行主序：`flat(p,j,*)=(p*4+j)*256`
- `ek = BE₁₂(t̂)[1536]‖ρ[32]`；`dk = BE₁₂(ŝ̂)[1536]`
- 握手：AIV Set(4)→Set(1)→Wait(3)；AIC Wait(4)→Wait(1)→Cube→Set(3)
