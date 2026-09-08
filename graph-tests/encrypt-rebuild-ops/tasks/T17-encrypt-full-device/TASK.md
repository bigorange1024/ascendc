# T17 — 全设备 Encrypt：CBD+Â+ŷ→u,v→c（无 Host 预喂噪声）

| 字段 | 值 |
|------|-----|
| 状态 | **ready** |
| DAG | `D-EXP-T17` |
| 代码目录 | `graph-tests/enc_related/RB-T17-encrypt-full-device/` |
| 运营目录 | `…/tasks/T17-encrypt-full-device/` |
| 墙钟 | ≤ 90 min CPU；编通即交，主控立刻 NPU |

继承 COMMON；**NPU 优先**。

## 目标

相对 T15：把 Host 预喂的 y（及尽量 e₁/e₂）改为 **设备 CBD(coins)**（T16 契约），再接 SampleNTT/NTT/Mul/INTT/pack→c。  
Host 仅 ek/coins/m（或 μ）与必要 LUT/ζ；**禁**预喂最终 Â/ŷ/u/v/c。  
主验收：c（及关键中间量）对拍；CPU 绿即交。

## 必读

T15/T16/T07 FEEDBACK；禁抄 encrypt/alg14/frozen；禁 5/7。

## 验收

`bash run.sh -r cpu -v Ascend910B4` → FEEDBACK PASS_CPU。
