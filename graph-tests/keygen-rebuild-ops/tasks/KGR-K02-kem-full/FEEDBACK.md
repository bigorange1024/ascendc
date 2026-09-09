ID: PASS
cmd: bash run.sh -r cpu -v Ascend910B4
exit: 0
wall_min: ~0.4（kernel≈5.4s；含编译总 ~24s）
sync_audit: clean（无红线；SYNC-05×2 薄封装假阳性 + SYNC-09 性能）
notes:
  - 新建 `graph-tests/kg_related/RB-K06-kem-full/`；四 launch Host mid-sync
  - basename：`kg_prep_custom`（链 K01）/`kg_ntt_custom`/`kg_dot_encode_custom`/`kg_kem_tail_custom`
  - 对拍：liboqs_kem_ref=`/home/yuanye/ascendc/scripts/liboqs_kem_ref`；SEED_D=20260619；z 域分离对齐 K01/fixture；ek/dk_kem max=0
  - L3 AIV-only BLOCK_DIM=1；禁与 L2b CrossCore 融合；禁抄 KeyGen；未改 KB/DAG
  - **npu: wait_npu**（云机关机；本刀未 SSH/-r npu）
next_hint: 主控开机后 P04/K01/K02 NPU×30；关 Q-KEM-KG / Q-KG-HANG
