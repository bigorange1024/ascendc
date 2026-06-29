# A_VECTOR_PLAN — Phase A（Alg.7 SampleNTT）向量化分阶段

**探针（历史名 `fix-f203-alg13-device-presample-a-hat-k4`）**：`frozen-fix-f203-alg13-device-presample-a-hat-k4`  
**讨论纪要**：[`qa/2026-06/2026-06-23-SampleNTT-PhaseA向量化讨论.md`](../../qa/2026-06/2026-06-23-SampleNTT-PhaseA向量化讨论.md)

---

## 目标

`SEED_D` → G（ρ,σ）→ **A** → P → C；在 `VERIFY_STAGE=all` 对拍前提下优化 Phase A tick。

**约束**：`blockDim=1`，单 AIV，不用 SIMT。

---

## 阶段一览

| 阶段 | 内容 | 状态 |
|------|------|------|
| **Phase 1** | 标量 PermuteChain 16× SampleNTT | ✅ SIM **719237** |
| **A-v1** | `shake_xof_kernel` batch=16 SHAKE128 | ✅ SIM **918301**（+27%） |
| **A-v2** | UB/GM rej；去掉 SetValue | ✅ 最快 shake_vec **715537** |
| **A-v3** | 惰性 tail squeeze（续流 168B） | ✅ |
| **A-v4a** | 48B 栈 + 标量 compact | ✅ SIM **960762**（负优化） |
| **A-v4b** | 48B + mask LUT compact | ✅ SIM **1004273**（负优化） |
| **A-v5** | UB 上 d1/d2 向量 unpack POC | **待开工** |
| **A-v6** | batch tail XOF 深化 | 排队 |

---

## 环境开关

| 变量 | 值 | 说明 |
|------|-----|------|
| `SE_A_HAT_STAGE` | `shake_vec`（默认）\| `scalar` | XOF 路径 |
| `SE_A_HAT_REJ` | `scalar`（默认）\| `vec_a` \| `vec_b` | rej 实现 |
| `SE_A_HAT_PROBE` | `full` \| `xof_only` \| `rej_only` | 分段门控 |
| `VERIFY_STAGE` | `all` \| `a_hat` \| `src` \| … | 验收范围 |

---

## 当前默认与结论

- **默认**：`SE_A_HAT_STAGE=shake_vec`，`SE_A_HAT_REJ=scalar`
- **SIM 基线（现行管线）**：**881627** tick（全段 G+A+P+C）
- **CAModel**：A-v4a/b **均未赢过** scalar；下一拐点在 **UB unpack POC** 或 **恢复 715k 档搬运** + **batch tail**

tick 详见 [`SIM_BENCHMARK.md`](SIM_BENCHMARK.md)。
