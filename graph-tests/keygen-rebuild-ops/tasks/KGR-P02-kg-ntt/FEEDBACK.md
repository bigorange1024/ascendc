ID: PASS_CPU
cmd: cd graph-tests/kg_related/RB-K02-kg-ntt && bash run.sh -r cpu -v Ascend910B4
exit: 0
wall_min: <1
sync_audit: clean（SYNC-05 高假阳性 + SYNC-09 性能；无红线）
notes:
- 测的是：KeyGen NTT(ŝ) 与 NTT(ê)（Alg.13 行 16）；不做点积/Encode/prep
- 对拍：host FIPS oracle — `topology_math.mlkem_ntt`（T10）；ŝ/ê=`golden_se_sampling.build_src(SEED_D=20260619)`
- 结果：s_ntt/e_ntt max=0；PASS_SYNC+PASS_IO；kernel wall≈0.5s
- MIX：flag 1/3+4=GATE；basename kg_ntt_custom；BLOCK_DIM=1；poly-batch；禁 Gather/limbsplit
- 壳参考 D02/T12 工程骨架；禁抄 KeyGen/Encrypt/Decrypt 整核
- 未改 KeyGen KB/DAG；未跑 SIM/NPU（本刀 subagent_cpu_sim）
next_hint: KGR-P03 Â∘ŝ+ê + ByteEncode；可选 SIM_DIRECT 补跑
