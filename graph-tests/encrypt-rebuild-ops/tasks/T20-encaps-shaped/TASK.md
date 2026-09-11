# T20 — Encaps 壳：m→μ + Encrypt 全链 → (c,K) 外形

| 字段 | 值 |
|------|-----|
| 状态 | **PASS_CPU**（待主控 NPU） |
| DAG | `D-EXP-T20` |
| 代码目录 | `graph-tests/enc_related/RB-T20-encaps-shaped/` |
| 运营目录 | `…/tasks/T20-encaps-shaped/` |
| 墙钟 | ≤ 90 min CPU |

继承 COMMON；**NPU 优先**。

## 目标

在 T19 Encrypt 全链之上加 **Encaps 外形**：
- Host 或设备：m → μ（T04 契约）；
- 跑通设备 Encrypt → c；
- Host 侧可先算 K=H(… ) 作对拍（设备 K 可后刀）。

主验收：c 对拍；尽量 K 对拍（若本刀含 Host K）。禁抄 encaps/alg14 树。

## 验收

CPU 绿 → FEEDBACK；主控立刻 NPU。
