# 2026-07-06 — Alg.14 Encrypt compute 行 18–19 单 launch 与 UB 驻留

**探针**：[`fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4`](../../ascendc-tests/fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4/)  
**定稿**：[`docs/notes/F203-Encrypt-compute-行18-19-UB驻留技术总结.md`](../../docs/notes/F203-Encrypt-compute-行18-19-UB驻留技术总结.md)  
**实现方案**：探针 [`INTEGRATION_PLAN.md`](../../ascendc-tests/fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4/INTEGRATION_PLAN.md) §8

---

## 1. 今日目标与范围

- **目标**：在 feasibility 探针上验证 Alg.14 **行 18（NTT(y)）+ 行 19（Âᵀ∘ŷ → INTT + e₁）** 的设备路径；默认 **3 launch** 已 PASS，推进 **`F203_FEAS_FUSED=1` 单 launch MIX**。
- **范围**：本日仅 **û / u**（4 poly）；**不含** tr̂ / v、行 2 decode、行 21。
- **前置**：prep 探针 [`fix-f203-alg14-lines3-15-encrypt-prep-k4`](../../ascendc-tests/fix-f203-alg14-lines3-15-encrypt-prep-k4/) 已 CPU+SIM PASS。

---

## 2. 验收结论（2026-07-06 晚）

| 模式 | 环境 | 结果 |
|------|------|------|
| 3 launch（`f203_encrypt_ntt_y` \| `at_jp` \| `intt_e1`） | CPU + SIM | `y_hat` / `u_ntt` / `u` **max=0** |
| 单 launch（`f203_encrypt_l18_l19`，`F203_FEAS_FUSED=1`） | SIM | 同上 **max=0**（~130s，`KERNEL_COMPUTE_BUDGET_SEC=180`） |
| 单 launch | CPU（tikicpu） | **不支持** — MIX 串行先跑 AIC 会死锁；`main` 早退并告警 |

---

## 3. 踩坑与根因

### 3.1 单 launch SIM 死锁（FSM）

- **现象**：host trace 停在 NTT MMAD 或 INTT 前，kernel 超时。
- **根因**：AIC 在内积未完成时进入 INTT；或仅一侧 AIV `SET(ST_IP_AIV_DONE)`。
- **修复**：对齐 stage123 / KeyGen 握手：
  - `ST_IP_AIV_DONE=4`：双 AIV 内积后各 `SET`；
  - `ST_AT_JP_GATE=8`：AIC `WAIT(4)` 后 `SET(8)`；双 AIV `WAIT(8)` 再进 INTT Stage1。
- **trace**：`main` 增 `AIC_AT_JP_GATE` / `AIV_AT_JP_GATE`；host poller 对 `aclrtMemcpy` 检查 `ACL_SUCCESS`（避免 SIM 活跃期 `107002` 刷屏）。

### 3.2 `u` 仅等于 `e₁ mod q`（非算法错）

- **现象**：`y_hat`、`u_ntt` golden 一致；`u` 像「INTT 输入全零 + e₁」。
- **探针**：`TR_DBG_*` 显示标量写 GM 的 `uNtt[p0][0]` 正确，但 `AivK8Split` 的 `DataCopy(GM→UB)` 读 S0 全零。
- **根因**：**同 kernel 内** 标量写 GM → MTE `DataCopy` 读 GM，**SIM 不可见**（与 KeyGen prep 同类教训；见 2s1e UB 融合 note）。
- **临时绕过**：`ProcessFromScalarGm`（标量逐 lane 搬 UB）— 已废弃。
- **定案**：**û 驻留 UB**（对齐 [`F203-2s1e-NTT内积UB融合技术总结.md`](../../docs/notes/F203-2s1e-NTT内积UB融合技术总结.md)）：
  - `innerproduct_halfrows_to_ub` → `LocalTensor`；
  - `AivK8Split::ProcessFromLocal(ubUNtt)`；
  - 仅对拍用 `dump_u_ntt_halfrows_ub`（`DataCopy` UB→GM）；
  - 同步：`PipeBarrier<PIPE_ALL>()` + INTT CrossCore **flag 1/3**（与 `intt_e1` 一致）。

### 3.3 CPU 单 launch

- tikicpu **串行**执行 MIX：先 AIC `WAIT` AIV 标志 → 永久阻塞。
- **决策**：feasibility 单 launch **仅 SIM**；3 launch 保留 CPU 快路径。

### 3.4 编译小项

- `LoadInttLutHostFused` CPU 未用 → `#ifndef ASCENDC_CPU_DEBUG` 包裹。
- `ProcessFromLocal` 传 `const LocalTensor&` → `split_vec` 不匹配 → 改非 const 引用。

---

## 4. 实现要点（文件）

| 文件 | 变更摘要 |
|------|----------|
| `compute/f203_encrypt_l18_l19_kernel.cpp` | FSM 4/8；内积→UB；INTT `ProcessFromLocal` |
| `compute/f203_encrypt_at_jp_scalar.hpp` | `innerproduct_halfrows_to_ub`、`dump_u_ntt_halfrows_ub` |
| `compute/aiv_func.hpp` | `ProcessFromLocal` + `encodeCore` 抽公共 |
| `main.cpp` | fused trace；CPU 单 launch 禁用；poller 健壮性 |
| `STATUS.md` | 可行性结论表 |

---

## 5. 讨论要点（用户 ↔ Agent）

1. **先学笔记再改码**：2s1e UB 融合、KeyGen prep SIM 可见性 — 避免重复造轮子。
2. **u 失败是搬运/可见性，不是 NTT 数学错** — 用分段 trace + golden 切片定位。
3. **测试阶段**：同步尽量 `PipeBarrier<PIPE_ALL>()`；搬运尽量 `DataCopy`；**中间态驻 UB** 是正路，GM 往返仅 debug dump。
4. **单 launch CPU 不支持** 是平台语义限制，不是「SIM 特供 hack」。

---

## 6. 下一步（未做）

| 项 | 说明 |
|----|------|
| **行 21 / v** | 扩展 at_y5 到 kP=5、`tr̂` INTT batch + e₂ |
| **行 2 decode** | `t̂` 设备 decode + matM 列 4 |
| **与 prep 拼接** | prep launch + compute launch → 目标 2 launch 全链 |
| **单 launch 含 v** | UB 预算核算；可能退 2 launch |
| **晋级 / stable** | 待 G5+ 全链 PASS 后再议 |

---

## 7. 验收命令

```bash
cd ascendc-tests/fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
F203_FEAS_FUSED=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```
