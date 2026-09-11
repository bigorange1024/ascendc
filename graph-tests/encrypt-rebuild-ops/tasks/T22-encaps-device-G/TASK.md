# T22 — Encaps：设备 (K‖r)←G(m‖H(ek))（接 T21）

| 字段 | 值 |
|------|-----|
| 状态 | **PASS_CPU**（Subagent 回填；待主控 NPU） |
| DAG | `D-EXP-T22` |
| 代码目录 | `graph-tests/enc_related/RB-T22-encaps-device-G/` |
| 运营目录 | `…/tasks/T22-encaps-device-G/` |
| 墙钟 | ≤ 90 min CPU |

继承 COMMON；**NPU 优先**。

## 目标

相对 T21：把 Host 的 `(K‖r)←G(m‖H(ek))` 尽量迁到设备（SHA3/G 积木）；coins/r 喂 CBD。  
主验收：c 与 K 对拍。禁抄 encaps/alg14/frozen；禁 5/7；`BLOCK_DIM=1`。

## 验收

CPU 绿 → FEEDBACK；主控立刻 NPU。
