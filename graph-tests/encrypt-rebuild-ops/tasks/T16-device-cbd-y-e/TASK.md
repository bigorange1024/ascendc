# T16 — 设备 CBD：coins→(y,e₁,e₂)（接 prep）

| 字段 | 值 |
|------|-----|
| 状态 | **ready** |
| DAG | `D-EXP-T16` |
| 代码目录 | `graph-tests/enc_related/RB-T16-device-cbd-y-e/` |
| 运营目录 | `…/tasks/T16-device-cbd-y-e/` |
| 墙钟 | ≤ 60 min（CPU）；主控立刻 NPU |

继承 COMMON；**NPU 优先**：默认交 CPU 绿即可，长 SIM 非门禁。

## 目标

Host 预喂 coins（对齐 T07）→ 设备 MIX：Alg.8 CBD η=2 产出 **y,e₁,e₂** 对拍。  
Flag 仅 1/3；永禁 5/7。可复用活跃 CBD 积木契约；禁抄 encrypt/frozen。

## 非目标

SampleNTT、NTT、u/v、pack、Encaps。

## 验收

`bash run.sh -r cpu -v Ascend910B4` → FEEDBACK PASS_CPU；主控上板。
