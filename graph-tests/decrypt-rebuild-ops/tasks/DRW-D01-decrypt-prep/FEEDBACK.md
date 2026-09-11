ID: PASS_CPU
cmd: cd graph-tests/dec_related/RB-D01-decrypt-prep && bash run.sh -r cpu -v Ascend910B4
exit: 0
wall_min: 20
sync_audit: clean（无 CrossCore；仅 SYNC-09 性能提示 PipeBarrier 粒度）
notes:
- 新建 `RB-D01-decrypt-prep/`；核入口 basename=`dec_prep_custom.cpp`；AIV-only、BLOCK_DIM=1、零 CrossCore。
- 功能：BD₁₂(dk_pke)→ŝ；c→u(d_u=11)、v(d_v=5)；写出 UB+DataCopy（X12）。
- BD₁₂ 用 `library/shared/f203_byte_codec/`；d=5/11 unpack+Decompress 本地 `dec_prep_helpers.hpp`（FIPS 契约，未抄 T25/alg15）。
- 对拍：ŝ/u/v 整缓冲 max=0 vs host Python FIPS oracle；**非 liboqs 权威**（TASK 允许；全链权威留给后续）。
- sync_audit.json → 本刀 `logs/`；无红线。
- 未跑 SIM/NPU（本刀门禁仅 CPU）。
next_hint: 派 DRW-D02（`dec_ntt_dot_custom`）；NPU 等用户开机后由主控推 D01/全链。

## 主控批注（2026-09-09）

- **采纳 PASS_CPU**：DG1 关闭（战役内 CPU）；oracle=host FIPS，**全链 liboqs 权威后置**。  
- basename / X12 / 零 CrossCore 合规。  
- **下一刀**：DRW-D02（L2a NTT+su_dot）；**仍不上 NPU**（云机未开）。

