ID: PASS_CPU
cmd: cd graph-tests/dec_related/RB-D02-decrypt-ntt-dot && bash run.sh -r cpu -v Ascend910B4
exit: 0
wall_min: 15
sync_audit: clean（无红线；SYNC-05 薄封装假阳性 + SYNC-09 性能提示，同 T12）
notes:
- 新建 `RB-D02-decrypt-ntt-dot/`；核入口 basename=`dec_ntt_dot_custom.cpp`；MIX AIC+AIV、BLOCK_DIM=1。
- CrossCore flag **1/3 + 4=GATE**；禁 5/7 / SoftSync；AIC Wait 环无 SyncAll。
- 功能：û←NTT(u)（Alg.9 poly-batch）；ŵ←Σ MultiplyNTTs(ŝ,û)（Alg.11）；写出 UB+DataCopy（X12）。
- 对拍：û/ŵ max=0 vs host FIPS oracle（T10 topology_math）；**非 liboqs 权威**。
- sync_audit.json → 本刀 `logs/`；无红线。
- 未跑 SIM/NPU（本刀门禁仅 CPU）。
next_hint: 派 DRW-D03（`dec_intt_extract_custom`）；NPU 等用户开机后由主控推。

## 主控批注（2026-09-09）

- **采纳 PASS_CPU**：DG2 关（战役内 CPU）；flag 1/3+4 合规；oracle=host FIPS。  
- **下一刀**：DRW-D03（L2b INTT+extract→m）；仍仅 CPU；云机未开。

