# T18 — 设备 ByteDecode₁₂(ek)→t̂（接 Encrypt prep）

| 字段 | 值 |
|------|-----|
| 状态 | **ready** |
| DAG | `D-EXP-T18` |
| 代码目录 | `graph-tests/enc_related/RB-T18-device-bytedecode12-ek/` |
| 运营目录 | `…/tasks/T18-device-bytedecode12-ek/` |
| 墙钟 | ≤ 50 min CPU |

继承 COMMON；**NPU 优先**。

## 目标

Host 预喂 ek（或 ek 体）→ 设备 MIX：ByteDecode₁₂ → t̂[4×256] 对拍。  
复用 T05 / shared decode12 契约；禁抄 encrypt 树。Flag 1/3；禁 5/7。

## 验收

CPU 绿即 FEEDBACK；主控立刻 NPU。
