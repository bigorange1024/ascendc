# T21 — Encaps：设备 μ←Decompress₁(m)（接 T20）

| 字段 | 值 |
|------|-----|
| 状态 | **ready** |
| DAG | `D-EXP-T21` |
| 代码目录 | `graph-tests/enc_related/RB-T21-encaps-device-mu/` |
| 运营目录 | `…/tasks/T21-encaps-device-mu/` |
| 墙钟 | ≤ 90 min CPU |

继承 COMMON；**NPU 优先**。

## 目标

相对 T20：μ 改由**设备** Decompress₁(m)（T04 契约），不再 Host 预喂最终 μ。  
其余 Encrypt 全链保持 T19/T20；K 仍可 Host G。主验收 c（及 μ）对拍。

## 验收

CPU 绿 → FEEDBACK；主控立刻 NPU。禁抄 encaps/alg14/frozen；禁 5/7。
