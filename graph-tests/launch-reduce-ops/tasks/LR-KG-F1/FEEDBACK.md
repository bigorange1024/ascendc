## RB-K07 NPU×30（2026-09-10）
```
ID: PASS_NPU_x30
cmd: ASCEND_DEVICE_ID=0 bash run.sh -r npu -v Ascend910B3 ×30
exit: 0
summary: ok=30 fail=0
wall: ~13min total（含每轮 run.sh）
log: /mnt/workspace/launch-reduce-logs/k07-npu-x30-20260910-094124.log
notes: PKE KeyGen Host 3→2 真机关闸；PASS_SYNC+PASS_IO 全程
next: KEM KeyGen 压 launch（自 RB-K06）；并行 Wave0 D04/T26 基线
```

## RB-K07 NPU 单轮（2026-09-10 重启后）
```
ID: PASS_NPU
cmd: cd graph-tests/kg_related/RB-K07-pke-2launch && ASCEND_DEVICE_ID=0 bash run.sh -r npu -v Ascend910B3
exit: 0
wall_sec: ~9.6（kernel）
notes:
- PASS_SYNC + PASS_IO；ek/dk max_abs=0 vs liboqs_pke_ref
- Host 2-launch fused MIX；flag 1/3/4 两段复用可达
- log: /mnt/workspace/launch-reduce-logs/k07-npu-20260910-094023.log
next: NPU×30 进行中
```

# LR-KG-F1 FEEDBACK（片段）

## RB-K07-pke-2launch 实现
```
ID: PASS_CPU（NPU 待跑）
cmd: cd graph-tests/kg_related/RB-K07-pke-2launch && bash run.sh -r cpu -v Ascend910B4
exit: 0
wall_sec: ~1.8（kernel 段）
sync_audit: 无红线；高×1（SYNC-05，CrossWait/CrossSet 薄封装假阳性，同 K04 SYNC-05/SYNC-03 已知模式）+ 性能×36（PipeBarrier<PIPE_ALL> 粒度）
notes:
- Host 3→2 launch：kg_prep_custom → mid-sync → kg_ntt_dot_encode_custom（单 MIX）→ sync
- 单 MIX 内串行两段握手，复用同一组 CrossCore flag {1,3,4}；段1 NTT(ŝ/ê) 结果由设备侧
  rb_k02::ComputeSeNtt 直写段2 S_NTT/E_NTT 槛位，无 Host 中转、无 SoftSync（无 GM 忙等）
- [PASS_SYNC] + [PASS_IO]：ek_pke/dk_pke max_abs=0 vs liboqs_pke_ref（SEED_D=20260619）
- NPU 未跑（云机无卡）；结案仍需有卡环境跑 `-r npu`
next_hint: 借入 NPU 后跑 `bash run.sh -r npu -v Ascend910B4`，确认 flag {1,3,4} 复用两遍在真实硬件可达
```
log: `graph-tests/launch-reduce-ops/tasks/LR-KG-F1/logs/sync_audit.json`
