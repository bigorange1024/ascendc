ID: PASS_CPU
cmd: cd graph-tests/dec_related/RB-D03-decrypt-intt-extract && bash run.sh -r cpu -v Ascend910B4
exit: 0
wall_min: 25
sync_audit: clean（无红线；SYNC-05 薄封装假阳性 + SYNC-09 性能提示，同 D02）
notes:
- 新建 `RB-D03-decrypt-intt-extract/`；核入口 basename=`dec_intt_extract_custom.cpp`；MIX AIC+AIV、BLOCK_DIM=1。
- CrossCore flag **1/3 + 4=GATE**；禁 5/7 / SoftSync；AIC Wait 环无 SyncAll；与 D02 分 launch。
- 功能：w←INTT(ŵ)（Alg.10）；m←ByteEncode₁(Compress₁(v−w))；写出 UB+DataCopy（X12）。
- 输入：ŵ[256]/v[256] int32 对齐 D02/D01；无 Tag5T pad。
- 对拍：m[32] max=0；w max=0 vs host FIPS（T10 topology_math + 统一整数 Compress₁）；**非 liboqs 权威**。
- sync_audit.json → 本刀 `logs/`；无红线。
- 未跑 SIM/NPU（本刀门禁仅 CPU）。
next_hint: 主控开拼装全链 Decrypt（三 launch + mid-sync）或先标 DG3 关；然后请用户开机做 NPU/liboqs。

## 主控批注（2026-09-09）

- **采纳 PASS_CPU**：DG3 关（战役内 CPU）。  
- **下一刀**：DRW-D04 三 launch 全链拼装（CPU）；绿后再请用户开机做 NPU/liboqs。

