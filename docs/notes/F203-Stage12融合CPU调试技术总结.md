# F203 Stage1+2 MIX 融合 — CPU 孪生调试技术总结

**读者**：调试 MIX 核 CrossCore + 多阶段融合的开发者  
**目的**：从 **同步与分阶段验证** 原理总结 CPU 挂死类问题；非逐步复述某次 exp  
**详细实录（PDF）**：[F203-Stage12融合CPU调试经验.pdf](F203-Stage12融合CPU调试经验.pdf)（TeX 源同目录）  
**状态**：相关 exp **已冻结**；路线被 Tag5T / 2s1e 取代

---

## 0. 本文怎么读

| 章节 | 内容 |
|------|------|
| §1 | MIX 融合的数据流契约 |
| §2 | CPU 孪生为何掩盖 / 暴露不同问题 |
| §3 | 分阶段隔离方法论 |
| §4 | 模式与 Checklist |
| §5 | 附录：本案现象与判决 |

---

## 1. MIX 融合数据流

单 MIX launch 内：

```text
AIV Stage1 写 workspace ──CrossCore──► AIC Stage2 读 workspace 写 mat_c
```

**不变量**：

- workspace 布局在 host tiling 与 device **同一 spec**  
- AIC **Wait(split)** 后才能读 Stage1 产物  
- 双 AIV 均参与 FSM，禁止一侧 silent skip  

---

## 2. CPU vs 真并行

| | CPU 孪生 | SIM/NPU |
|---|----------|---------|
| MTE/Cube/Vector 并行 | 近似顺序 | 真并行 |
| 缺 barrier | 可能仍「跑完」 | 挂死、脏读、无限等 |
| 调试时长 | 易误判为「慢但正常」 | 需 **计算预算** 内出结果 |

**原则**：CPU 通过 **不能** 删除 SIM 必需的 `PipeBarrier` / CrossCore。

---

## 3. 分阶段隔离（Stage Gate）

对 \(F = \mathrm{Stage2} \circ \mathrm{Stage1}\)：

1. **仅 Stage1** AIV 探针 — golden encode  
2. **仅 Stage2** 预设 `mat_a` — golden matmul  
3. **再** 单 launch 融合  

失败在 (3) 而 (1)(2) 均过 → 搜 **同步 / workspace 可见性**，不是重新推公式。

**时间预算**：纯计算应在约定秒级内出现 `[SUCCESS]`；数十分钟无输出 = **同步或死等**，不是「再多等」。

---

## 4. 模式 P-mix-debug-1

| 步骤 | 动作 |
|------|------|
| 1 | 确认隔离基线 PASS |
| 2 | 打印 tiling / workspace 偏移 |
| 3 | 融合版对比：是否多 Wait、少 Barrier |
| 4 | 必要时 **两趟 launch** 作 CPU 调试手段（非 SIM 验收） |
| 5 | 判决：修同步 vs 放弃该融合壳 |

---

## 5. 附录：本案摘要

| 项 | 内容 |
|----|------|
| 用例 | `exp-mlkem-f203-stage12-encode-matmul-mix`（frozen） |
| 现象 | CPU 在 `[TmSim]` 后长时间无输出 |
| 对比 | Stage2 隔离 <10s 成功 |
| 判决 | NTT 内 `Matmul<>` 融合路线废弃；见 [NTT-Matmul路线废弃说明.md](NTT-Matmul路线废弃说明.md) |
| 勘误 | `exp-sepolyvec8-ntt-k8` 全链 CPU 亦未在预算内通过（曾误记） |

---

*2026-06-18：新增原理层 MD；细节见 TeX/PDF。*
