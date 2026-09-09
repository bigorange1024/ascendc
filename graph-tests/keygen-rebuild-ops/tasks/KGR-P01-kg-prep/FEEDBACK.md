ID: PASS_CPU
cmd: cd graph-tests/kg_related/RB-K01-kg-prep && bash run.sh -r cpu -v Ascend910B4
exit: 0
wall_min: <1
sync_audit: clean（仅 SYNC-09 PIPE_ALL 粒度提示；无 CrossCore 红线）
notes:
- 测的是：KeyGen prep（ρ/σ + Â + ŝ/ê）；不做 NTT/点积/Encode
- 对拍：host Python oracle（alg7_geom SampleNTT + golden_se_sampling CBD；FIPS203_PRF_BACKEND=shake256）
- 结果：a_hat/s_hat/e/src/prf_out 全部 max=0；kernel wall≈4.3s
- Seed：SEED_D=20260619；Derand 域前缀 exp-mlkem-f203-2s1e-k4
- 积木：lines3-7 + lines8-15 + alg8（-I only）；basename kg_prep_custom；blockDim=1 AIV-only
- 禁抄：未碰 examples/*keygen* / pass-fix*keygen* / frozen KeyGen
next_hint: KGR-P02 NTT(ŝ/ê)；可选 SIM_DIRECT 补跑；双 AIV Â 并行须 ProcessInline 论证后再开
