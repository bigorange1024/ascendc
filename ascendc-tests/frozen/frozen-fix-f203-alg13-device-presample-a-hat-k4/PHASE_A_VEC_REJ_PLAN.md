# PHASE_A_VEC_REJ_PLAN — rej_uniform 向量化设计稿

**状态**：A-v3 ✅ · A-v4a ✅ · A-v4b ✅（均为 SIM 负优化，保留对照）  
**讨论**：[`qa/2026-06/2026-06-23-SampleNTT-PhaseA向量化讨论.md`](../../qa/2026-06/2026-06-23-SampleNTT-PhaseA向量化讨论.md)

---

## 1. 目标管线（终态）

```text
G → ρ, σ
A2  shake_xof_kernel batch=16 → xof_out[16,504]
A3a 向量 unpack：504B → d1[168], d2[168]（或 48B→cand[32]）   ← 待 POC
A3b Compare + compact → a_hat[16,256]
A3c tail：while (any ctr<256) batch squeeze(168B)              ← A-v3 部分完成
P / C  母探针不变
```

---

## 2. 已完成实验（A-v4）

| 变体 | 文件 | 要点 | SIM |
|------|------|------|-----|
| scalar | `f203_a_hat_scalar.hpp` + `f203_a_hat_ub.hpp` | GM 栈 `rowBuf[504]` + 标量 rej | **881627** |
| vec_a | `f203_a_hat_rej_vec_a.hpp` | 48B 栈解包 + 标量 compact | **960762** |
| vec_b | `f203_a_hat_rej_vec_b.hpp` + `f203_rej_uniform_table.h` | mask8 → 语义 LUT | **1004273** |

LUT 生成：`scripts/gen_rej_uniform_table.py`（**语义** mask→cand 下标，非 x86 `pshufb` 字节）。

---

## 3. 对标 x86/ARM

| AVX2/NEON | AscendC 映射 | 本项目状态 |
|-----------|--------------|------------|
| 48B load + shuffle 解 12-bit | 宽载 + Shift/Or 或 48B 块解包 | v4a 仅标量栈解包 |
| `cmpgt` / `cmhi` | `Compares` | SIM 上曾挂死；v4a 用标量 compare |
| `rej_uniform_table` + shuffle/tbl | 语义 LUT + 标量索引写 | v4b ✅ 功能，SIM 更慢 |
| `popcnt` 更新 ctr | 标量 popcount | v4b |

---

## 4. 下一版设计（A-v5，未实现）

**原则**：unpack 与 rej **分阶段**；禁止 `GetValue`/`SetValue` 主路径；**批量**掩码+交错+compact+取前 256（空间换时间，输出与规范等价，见 qa §13.5）。

**单 poly 实现方案**：[`pass-fix-f203-alg7-sample-ntt-k4/INTEGRATION_PLAN.md`](../pass-fix-f203-alg7-sample-ntt-k4/INTEGRATION_PLAN.md)

### 4.1 d1/d2 unpack（Alg.7 步骤 6–7）

- **状态**：[`pass-fix-f203-alg7-sample-ntt-k4`](../pass-fix-f203-alg7-sample-ntt-k4/) CPU/SIM ✅
- 504B = 168 × (C0,C1,C2)；UB 向量 unpack；SIM 解交织从 GM 读

### 4.2 rej + compact（A-v5：Min+mod 批量路径）

| 步 | 原语 | 说明 |
|----|------|------|
| 掩码 | `Mins(d,q)` + lane 清零 | 优先于 `Compares`（SIM 历史风险） |
| 交错 | ROM + `Gather`（n=168） | Alg.11 `interleave_pairs_datacopy` 同构 |
| compact | 先标量门禁 R3，再 64-pair tile 向量 | A-v4 教训：半向量 compact 曾 +9%~+14% |
| 输出 | `â[256]` = compact 前缀 | 多块 tail squeeze 见 INTEGRATION_PLAN §3.5 |

### 4.3 反模式（已验证）

- UB `GetValue`/`SetValue` 假向量（>600s SIM 无输出）
- x86 `pshufb` LUT 直接当 cand 下标（CPU FAIL）
- 48B 栈块 + 标量 compact 当「向量 rej」（+9%~+14% tick）

---

## 5. 验收

```bash
SE_A_HAT_REJ=scalar|vec_a|vec_b bash run.sh -r cpu -v Ascend910B4
SE_A_HAT_PROBE=rej_only bash run.sh -r sim -v Ascend910B4
```
