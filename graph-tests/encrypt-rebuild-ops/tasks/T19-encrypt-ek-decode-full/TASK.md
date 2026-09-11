# T19 — Encrypt 全链：ek Decode₁₂ + CBD + Â/ŷ → u,v → c

| 字段 | 值 |
|------|-----|
| 状态 | **ready** |
| DAG | `D-EXP-T19` |
| 代码目录 | `graph-tests/enc_related/RB-T19-encrypt-ek-decode-full/` |
| 运营目录 | `…/tasks/T19-encrypt-ek-decode-full/` |
| 墙钟 | ≤ 90 min CPU |

继承 COMMON；**NPU 优先**。

## 目标

相对 T17：Host 不再预喂 t̂，改为设备 **ByteDecode₁₂(ek)→t̂**（T18），再 CBD+SampleNTT+NTT→u,v→c。  
Host 仅 ek/coins/m（μ）与 LUT/ζ。主验收 c（及关键中间量）对拍。

## 必读

T17/T18 FEEDBACK；禁抄 encrypt/alg14/frozen；禁 5/7。

## 验收

CPU 绿 → FEEDBACK PASS_CPU → 主控立刻 NPU。
