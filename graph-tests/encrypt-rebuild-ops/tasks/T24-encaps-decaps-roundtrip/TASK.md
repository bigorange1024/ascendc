# T24 — Encaps→Decaps 往返（K 一致）

| 字段 | 值 |
|------|-----|
| 状态 | **ready** |
| DAG | `D-EXP-T24` |
| 代码目录 | `graph-tests/enc_related/RB-T24-encaps-decaps-roundtrip/` |
| 运营目录 | `…/tasks/T24-encaps-decaps-roundtrip/` |
| 墙钟 | ≤ 90 min CPU |

继承 COMMON；**NPU 优先**。

## 目标

- AscendC Encaps（T22/T23 路径）出 `(c,K)`；
- Decaps：优先 **liboqs Decaps(sk,c)→K'** 与 K 对拍（权威往返）；设备 Decaps 可后刀。
- 禁抄 decaps/alg14/alg21 树；可用 liboqs / shared。

## 验收

CPU：`K==K'`；FEEDBACK；主控立刻 NPU。
