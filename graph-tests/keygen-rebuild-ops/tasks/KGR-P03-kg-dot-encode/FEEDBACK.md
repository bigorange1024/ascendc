ID: PASS
cmd: bash run.sh -r cpu -v Ascend910B4
exit: 0
wall_min: ~0.3（kernel≈0.58s；含编译总 ~17s）
sync_audit: clean（无红线；SYNC-05 薄封装假阳性 + SYNC-09 性能×29）
notes:
  - 新建 `graph-tests/kg_related/RB-K03-kg-dot-encode/`；basename `kg_dot_encode_custom`
  - MIX AIC_1_2；BLOCK_DIM=1；flag 1/3+4=GATE；业务 UB+DataCopy（X12）
  - AIV0：t̂=Â∘ŝ̂+ê̂（Alg.11 4×4）→ BE₁₂ → ek[1568]/dk[1536]
  - oracle：host FIPS（SEED_D=20260619；SampleNTT+mlkem_ntt+multiply_ntts+BE₁₂；非 liboqs）
  - ek/dk/t_hat 对拍 max=0；PASS_SYNC+PASS_IO
  - 禁抄 KeyGen/Encaps/Decrypt 整核；壳参考 K02/D02；未改 KB/DAG
next_hint: KGR-P04 三 launch 全链 + liboqs ek/dk 交叉；主控可排 NPU
