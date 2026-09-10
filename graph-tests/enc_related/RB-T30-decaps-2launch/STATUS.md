# RB-T30-decaps-2launch · STATUS

| 项 | 值 |
|----|-----|
| 刀 | LR-DP-F3（Decaps → **2** Host launch 实验） |
| Kernels | `t30_dec_fused_custom` · `t30_enc_fused_custom` |
| Host launch | **2** |
| Magic out | `0x54333032` ("T302") |
| Flag | L1 ∈{1,3}；L2 ∈{1,3}+4；禁 SoftSync；SyncAll 仅 prep 后（Wait 环外） |
| 约束 | **独立新树**；不改 T28/T29/D08/D09/stable/frozen；禁抄核源码 |
| 状态 | **CPU PASS + SIM_DIRECT PASS**（2026-09-10 Cloud）；NPU×30 待上板 |

## Host 路径

1. `ACLRT_LAUNCH_KERNEL(t30_dec_fused_custom)(…)` → Sync → m'
2. `ACLRT_LAUNCH_KERNEL(t30_enc_fused_custom)(…)` → Sync → c'/K'
3. Host FO → K；写 `launch_count.txt=2`

## 验收清单

| 档 | 期望 | 结果 |
|----|------|------|
| gen_data | liboqs → dk/c/ek + golden_K | PASS |
| verify | K≡golden；magic；TRACE；launches=2 | PASS |
| CPU | `bash run.sh -r cpu -v Ascend910B4` | **PASS**（~2.4s wall） |
| SIM_DIRECT | `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` | **PASS**（~401s wall；用例根无 stray dump） |
| NPU×30 | 战役结案真源 | 待上板 |
