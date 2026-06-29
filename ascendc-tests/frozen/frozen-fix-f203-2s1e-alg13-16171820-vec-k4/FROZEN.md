# ⛔ 已冻结（2026-06-18）

**原路径**：`ascendc-tests/fix-f203-2s1e-alg13-16171820-vec-k4/`  
**继任**：[`pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2`](../../pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/) — **活跃 MLKEM 行 16–20 向量集成基线**

## 冻结原因

| 类别 | 说明 |
|------|------|
| **行 18 路线过时** | 本目录 `stageHatInto` 走 **half→p→j** + `multiply_ntts_half_vec`（半多项式块 + `a_tile` 预搬），未采用内积探针验证的 **j→p 全 poly `compute_on_ub`** 路径。 |
| **SIM 性能不达标** | 全链路 `mixPass=0` SIM tick **~137k**（早期集成测量），超出 **<100k** 红线；继任 v2 全链路（ê + ByteEncode）**86120 tick**。 |
| **UB 占用与 TPipe 模型** | 行 18 在统一 `scratch_` 内叠加大块 hat 工作区（ROM + VecWs + half 累加 + `aHatAivTile`），与 S3 后 ŝ 驻留 **单 slim TPipe** 的融合不变量冲突；v2 用独立 `dotScratchBuf_` + ROM `kRomPairCount=128` 闭合。 |
| **功能已被 v2 超集取代** | v2 在同等 golden 契约下完成 dot-only / +ê / +ByteEncode 分阶段验收；本目录无独立维护价值。 |
| **文档与决策已迁移** | 技术总结、qa 纪要、INDEX 活跃基线均指向 **vec-k4-v2**；本目录仅作「half-basemul 拼模块」历史对照。 |

## 放弃决策（2026-06-18）

1. **不再维护** 本目录作 MLKEM 向量集成入口；新任务从 **vec-k4-v2** 出发。  
2. **不再参考** 本目录 `stageHatInto` half 路径作融合实现模板（可读 `INTEGRATION_PLAN.md` 了解模块拼接思路，**禁止抄码**）。  
3. **标量对照组**已归档 [`frozen-fix-f203-2s1e-alg13-16171820-k4`](../../frozen/frozen-fix-f203-2s1e-alg13-16171820-k4/)（2026-06-19）；向量全链路走 **v2**。

## 历史价值（只读）

- 首次将 **Alg11 向量 basemul** 与 **ByteEncode₁₂ 向量** 并入 `Aiv2s1eUbPipeline` 单 kernel；证明模块级 PASS ≠ 融合 tick 达标。  
- 记录 half-basemul + 大 scratch 在全链路下的 tick 代价，促成 v2 内积 UB 融合重构。

**勿 fork、勿抄码、勿跑 CI。**
