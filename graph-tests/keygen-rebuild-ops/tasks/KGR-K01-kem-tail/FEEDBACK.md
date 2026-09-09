ID: PASS
cmd: bash run.sh -r cpu -v Ascend910B4
exit: 0
wall_min: ~0.3（kernel≈1.05s；含编译总 ~16s）
sync_audit: clean（无红线；仅 SYNC-09 性能）
notes:
  - 新建 `graph-tests/kg_related/RB-K05-kem-tail/`；basename `kg_kem_tail_custom`；AIV-only；BLOCK_DIM=1
  - 对拍：host oracle（SHA3-256(ek)+z 域分离+拼接）；H/z/dk_kem max=0
  - z=`SHA3-256("exp-mlkem-f203-kem-k4:SEED_Z=20260619")`；上游 RB-K04 ek/dk_pke
  - 禁抄 KeyGen；未 fork Encaps 整核；X12 DataCopy；未改 KB/DAG
  - **npu: wait_npu**（云机关机；本刀未 SSH/-r npu）
next_hint: 主控开机后可 NPU；或开 KGR-K02 四 launch 全链 + liboqs_kem_ref
