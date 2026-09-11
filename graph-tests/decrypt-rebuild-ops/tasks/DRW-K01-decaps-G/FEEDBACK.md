ID: PASS_CPU
cmd: cd graph-tests/dec_related/RB-D04-decaps-G && bash run.sh -r cpu -v Ascend910B4
exit: 0
wall_min: ~5（编+跑≈18s；kernel≈1.05s）
sync_audit: clean（无红线；仅 SYNC-09 性能提示 PipeBarrier 粒度）
notes:
- 新建 `RB-D04-decaps-G/`（与 `RB-D04-decrypt-full` 并存）；basename=`decaps_g_custom.cpp`；AIV-only、BLOCK_DIM=1、零 CrossCore。
- 功能：设备 G=SHA3-512(m'‖h)→(K'‖r')；h 契约=dk_kem[3104:3136) 切片（gen 先造 dk 再切片写 h.bin；禁默认可重算 H(ek)）。
- 写出 UB+DataCopy（X12）；Host 清零输出，不预喂 K'/r'。
- 对拍：K'/r' 各 32B max=0 vs host hashlib.sha3_512；**非 liboqs 权威**（TASK 允许；NPU/交叉留给主控）。
- sync_audit.json → 本刀 `logs/` 与用例 `logs/`；无红线。
- 未跑 SIM/NPU（本刀门禁仅 CPU；禁 SSH/NPU）。
next_hint: 请主控开机后推 `-r npu`

## 主控批注（2026-09-09）

- **采纳 PASS_CPU**：AIV-only G；切片 h；sha3_512 oracle 可接受（G 即 SHA3-512）。

### 主控 NPU（同日，`cannlab-npu` 910B3）

| 项 | 结果 |
|----|------|
| 命令 | 侧树 rsync + `flock` `bash run.sh -r npu -v Ascend910B3` |
| 对拍 | K'/r' max=0（`hashlib.sha3_512`）；`wall_sec=3.258` / 180 |
| 结论 | **DG4 关**；开 **DRW-K02** |

ID: PASS_NPU
exit: 0
wall_sec: 3.258

## 日志索引

| 文件 | 说明 |
|------|------|
| [`logs/sync_audit.json`](logs/sync_audit.json) | cannbot sync_audit |
| 用例 `STATUS.md` | [`../../../dec_related/RB-D04-decaps-G/STATUS.md`](../../../dec_related/RB-D04-decaps-G/STATUS.md) |

## 主控补注（2026-09-09）

- **NPU×30 lean**：ok=30 fail=0；`ALL_DONE_K01_X30` @ 16:51:24。
