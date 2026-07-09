# 2026-06-19 — ByteEncode₁₂ prefetch / 块紧凑 S0（纪要桩）

> **说明**：原日纪要正文未随仓库保留；定稿结论已迁入 `docs/notes/`。本文件作**索引锚点**，避免 INDEX / 探针文档死链。

## 定稿去向

| 主题 | 文档 |
|------|------|
| ByteEncode₁₂ prefetch、encode-only tick、合入 vec-k4-v2 | [`docs/notes/F203-ByteEncode12-prefetch技术总结.md`](../../docs/notes/F203-ByteEncode12-prefetch技术总结.md) |
| 块紧凑 S0 路线否决 | [`examples/frozen/frozen-exp-mlkem-sepolyvec8-ntt-k4-block/`](../../examples/frozen/frozen-exp-mlkem-sepolyvec8-ntt-k4-block/) · [`ascendc-tests/frozen/INDEX.md`](../../ascendc-tests/frozen/INDEX.md) |
| 全链路 SIM 基线 | [`pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2`](../../ascendc-tests/pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/)（SIM **77958**） |

## 历史章节锚点（兼容旧链接）

- §8–§9：prefetch 合入与 SIM 复测 → 见上表 prefetch 技术总结 / `SIM_BENCHMARK.md`
- §10：块紧凑 S0 否决 → 见 `examples/frozen/…-block/` 与 frozen INDEX
- §12–§14：设备 PRF 两阶段 → 见 KeyGen / SampleNTT 相关 notes（后续会话）
