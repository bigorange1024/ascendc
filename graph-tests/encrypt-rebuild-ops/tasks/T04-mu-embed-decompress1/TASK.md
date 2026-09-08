# T04 — μ_embed / Decompress₁（G1）

| 字段 | 值 |
|------|-----|
| 状态 | **dispatched** |
| DAG | `D-EXP-T04` → 关闭 `G-MU` |
| 代码目录 | `graph-tests/bricks/RB-T04-mu-embed/` |
| 运营目录 | `…/tasks/T04-mu-embed-decompress1/` |
| 墙钟 | ≤ 40 min |

继承 COMMON。

## 目标

独立验收：**m[32] bytes → μ[256] int16/int32（FIPS Decompress₁）**。  
优先：Host golden + 设备 AIV-only 或 CPU 孪生对拍；**可不建 MIX**（无 CrossCore 则 sync_audit 可声明 N/A）。

## 非目标

并进 Encrypt launch；从旧 `f203_mu_embed` **抄实现**（契约可查 notes，代码自写）。

## 必读

- inventory **G1** · KB §A1 μ 段  
- `F203-Compress-Decompress-向量实现指南.md`（Decompress 原理；d=1 自实现）  
- FIPS 203 Decompress₁ 公式

## 验收

- golden：固定 m → μ；`cmp` 或逐元素 max=0  
- CPU；若有设备核则加 SIM  
- FEEDBACK；建议是否进 `library/shared`（**仅建议，勿擅自大迁**）

## 依赖

无（Wave B）。
