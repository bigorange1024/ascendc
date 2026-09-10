# STATUS — RB-D09-decrypt-1launch

| 字段 | 值 |
|------|-----|
| 刀 | LR-DC-F2 · Decrypt 一 launch |
| 状态 | **CREATED**（源码落盘；待 NPU 验收） |
| Kernel | `d09_decrypt_fused_custom` |
| Namespace | `d09` / `rb_d09` |
| Magic out | `0x44303931` ("D091") |
| Flag | ∈{1,3}；禁 SoftSync；SyncAll 仅 prep 后（Wait 环外） |
| BLOCK_DIM | 1 · `KERNEL_TYPE_MIX_AIC_1_2` |

## Host

单 launch：`ACLRT_LAUNCH_KERNEL(d09_decrypt_fused_custom)(blockDim, stream, out, mOut, dkDev, cDev, wsDev, tiling)` → Sync → D2H m/TRACE/out。

## Device

AIV0 PrepUnpack → ALL SyncAll → AIC/AIV 两轮 flag1/3（NTT+dot / INTT+extract）→ MAGIC。

## 验收命令

```bash
cd graph-tests/dec_related/RB-D09-decrypt-1launch
bash run.sh -r npu -v Ascend910B4
```
