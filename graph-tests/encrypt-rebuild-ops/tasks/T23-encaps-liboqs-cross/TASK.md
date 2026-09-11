# T23 — Encaps × liboqs 交叉（权威 c/K）

| 字段 | 值 |
|------|-----|
| 状态 | **PASS_CPU**（Subagent 回填；待主控 NPU） |
| DAG | `D-EXP-T23` |
| 代码目录 | `graph-tests/enc_related/RB-T23-encaps-liboqs-cross/` |
| 运营目录 | `…/tasks/T23-encaps-liboqs-cross/` |
| 墙钟 | ≤ 90 min CPU |

继承 COMMON；**NPU 优先**。

## 目标

在 T22 设备 Encaps 路径上加 **liboqs 交叉**：
- 固定/urandom `m` + 合法 `ek`（可 Host KeyGen/liboqs 生成）；
- AscendC 出 `c`/`K`；
- 与 liboqs ML-KEM-1024 Encaps **同输入逐字节**对拍（或 Decaps 往返 K）。

禁抄 encaps/alg14 树实现；可链接 `thirdparty/liboqs` / 仓内 ref 胶水。缺 liboqs → FEEDBACK BLOCKED（勿假绿）。

## 验收

CPU 绿（交叉过）→ FEEDBACK；主控立刻 NPU。
