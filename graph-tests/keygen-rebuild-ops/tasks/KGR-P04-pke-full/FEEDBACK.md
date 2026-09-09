ID: PASS
cmd: bash run.sh -r cpu -v Ascend910B4
exit: 0
wall_min: ~0.4（kernel≈5.2s；含编译总 ~23s）
sync_audit: clean（无红线；SYNC-05×2 薄封装假阳性 + SYNC-09 性能）
notes:
  - 新建 `graph-tests/kg_related/RB-K04-pke-full/`；三 launch Host mid-sync
  - basename：`kg_prep_custom`（链 K01）/`kg_ntt_custom`/`kg_dot_encode_custom`
  - 对拍：liboqs_pke_ref=`/home/yuanye/ascendc/scripts/liboqs_pke_ref`；SEED_D=20260619；ek/dk max=0
  - 禁抄 KeyGen；k02_inc/k03_inc 隔离；未改 KB/DAG
  - **npu: wait_npu**（云机关机；本刀未 SSH/-r npu）
next_hint: 主控开机后 NPU×30；或并行开 KGR-K01（若采纳本 PASS_CPU）
