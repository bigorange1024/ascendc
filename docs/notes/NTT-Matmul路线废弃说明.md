# NTT 内 Matmul\<> 路线废弃说明

**读者**：评估「在 NTT MIX 里用高级 Matmul 融合」的 Agent  
**目的**：说明 **为何** 该路线被判决关闭，而非仅列出 frozen 目录名  
**canonical 讨论**：[qa/2026-06/2026-06-11-…#NTT-Matmul](../../qa/2026-06/2026-06-11-ascendc-engineering-notes与数据搬运.md#ntt-matmul路线废弃冻结)  
**治理**：[研究路线与frozen治理.md](研究路线与frozen治理.md)

---

## 0. 一句话判决

**在 NTT 三段式 MIX 内，用 `Matmul<>` / LeakyRelu 融合模板替代手写 `AicMmad`，路线废弃冻结。** 继任：merged_kyber FSM + `AicMmad`（本身亦已 frozen）→ ML-KEM Tag5T / 2s1e。

---

## 1. 数学上没问题，工程上不过关

NTT Stage2 本质是 **固定形状的 int8 矩阵乘**。`Matmul<>` 在 **孤立 Cube 探针** 上可以 CPU/SIM 通过。

失败发生在 **多段 Host 接入 + MIX CrossCore**：

| 假象 | 实际 |
|------|------|
| CPU 两段式 launch 通过 | SIM 全链路挂死或结果错 |
| 孤立 `Matmul<>` kernel SIM 通过 | 与 AIV encode 段拼接后 GM 可见性/同步不满足 |
| 模板「应该处理好」同步 | 须显式 `CrossCoreWait` + `PipeBarrier` |

**不变量**：复合算子 \(F = G \circ H\) 的验收必须是 **\(F\) 整体**，不能由「\(H\) 孤立通过」推断 \(F\) 可交付。

---

## 2. 与通用 DL Matmul 的边界

| 场景 | Matmul\<> |
|------|-----------|
| 独立 int8 GEMM 探针、非 NTT 壳 | 可调研（见 [AscendC-多核MatMul-tiling技术总结.md](AscendC-多核MatMul-tiling技术总结.md)） |
| NTT MIX 内 Stage2 | **禁止** |
| NTT 后 Alg.13 行 18（NTT 域 basemul） | 用专用 basemul 核，不是 Cube Matmul\<> |

---

## 3. Agent 行为

1. 搜索命中 `frozen-*-matmul`、`frozen-exp-mlkem-f203-stage12-encode-matmul-mix` → 读 `FROZEN.md`  
2. **禁止** fork / 抄码 / 文档写「参考 frozen-xxx Matmul 实现」  
3. 活跃 NTT 路径：[MLKEM-NTT-实现总结.md](MLKEM-NTT-实现总结.md)

---

## 4. 附录：冻结索引

- [ascendc-tests/frozen/INDEX.md](../../ascendc-tests/frozen/INDEX.md)
- [examples/frozen/INDEX.md](../../examples/frozen/INDEX.md)

*原 `docs/research/20260611-NTT-Matmul路线废弃冻结.md`；2026-06-18 迁入 notes。*
