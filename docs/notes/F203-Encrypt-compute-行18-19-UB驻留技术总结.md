# Encrypt compute 行 18–19：内积→INTT 的 UB 驻留 — 技术总结

**读者**：未参与本仓库开发的实现者 / Agent  
**目的**：说明 Encrypt 线性段 **内积产出 û 如何无 GM 往返地喂给 INTT Stage1**，以及 SIM 上标量写/MTE 读的可见性陷阱  
**案例锚点**：`ascendc-tests/fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4`（§6 附录）  
**讨论**：[`qa/2026-07/2026-07-06-Encrypt-compute单launch与UB驻留.md`](../../qa/2026-07/2026-07-06-Encrypt-compute单launch与UB驻留.md)  
**实现方案**：探针 [`INTEGRATION_PLAN.md`](../../ascendc-tests/fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4/INTEGRATION_PLAN.md) §8  
**上游原理**：[`F203-2s1e-NTT内积UB融合技术总结.md`](F203-2s1e-NTT内积UB融合技术总结.md)

---

## 0. 本文怎么读

| 章节 | 内容 | 依赖代码名 |
|------|------|------------|
| §1 | 数学段边界（行 18–19） | 否 |
| §2 | 融合不变量：生产者–消费者同驻留 | 否 |
| §3 | MIX FSM 与 CrossCore 握手 | 少量 |
| §4 | SIM 标量写 vs MTE 读 GM | 否 |
| §5 | 可复用模式 | 否 |
| §6 | 本探针附录 | 是 |

---

## 1. 数学与数据契约

### 1.1 本段输入输出

- **输入**：`ŷ = NTT(y)`（k=4 polyvec，NTT 域 int32）；`Â`（`a_hat` GM）；`e₁`（时域）。
- **行 19 前半**：`û[p] = mod_q( Σ_j MultiplyNTTs(A[j,p], ŷ[j]) )`，p∈{0..3}。
- **行 19 后半**：`u = INTT(û) + e₁`（时域 mod q）。

本 feasibility 探针 **暂不计算** `tr̂`（第 5 列）与 `v`。

### 1.2 与 2s1e 的关系

2s1e 解决 **NTT(ŝ) 与行 18 内积** 的 UB 融合。本段解决 **内积与 INTT** 的衔接：消费者是 INTT Stage1（limb split），不是 basemul。不变量相同：**权威中间态必须在生产者当次写出的缓冲上被消费**。

---

## 2. 工程不变量

| 编号 | 不变量 |
|------|--------|
| **R1** | 内积写出的 `û` 与 INTT Stage1 读的系数 **同一物理缓冲**（UB 或经 `PipeBarrier` 闭合的 UB 视图） |
| **R2** | 禁止「标量写 GM 中间态 → 同 kernel MTE `DataCopy` 读 GM」作为融合主路径（SIM 可见性不可靠） |
| **R3** | GM 落盘 `u_ntt` **仅**用于 golden 对拍，不得作为 Stage1 输入源 |
| **R4** | MIX 段间：全部参与方完成上一段后，AIC 才能启动下一段 MMAD（双 AIV 握手） |

---

## 3. AscendC 落地模型

### 3.1 三 launch 基线（可 CPU）

```text
Launch1: NTT(y)     → y_hat GM
Launch2: at_jp      → u_ntt GM（launch 边界 = 天然 sync）
Launch3: INTT+e₁    → u GM
```

launch 边界等价 `aclrtSynchronizeStream`，无同 kernel 可见性问题。

### 3.2 单 launch 融合（仅 SIM）

```text
AIV: NTT(y) S1–S3 → y_hat GM
AIV: innerproduct_halfrows_to_ub → ubUNtt (LocalTensor)
AIV: AivK8Split::ProcessFromLocal(ubUNtt)   // INTT S1，无 GM 读
[可选] dump_u_ntt_halfrows_ub → GM          // 仅对拍
CrossCore: IP_DONE(4) / AT_JP_GATE(8)
AIC: INTT S2 MMAD
AIV: INTT S3 + add e₁ → u GM
```

**CrossCore 旗语**（本探针锁定）：

| 值 | 语义 |
|----|------|
| 1, 2, 3 | NTT S1/S2/S3 |
| 4 | 双 AIV 内积完成 `ST_IP_AIV_DONE` |
| 8 | AIC 放行 INTT `ST_AT_JP_GATE` |
| INTT 内 | flag **1 / 3**（与 `intt_e1`、stage123 一致） |

段内同步：`KYBER_PIPE_ALL()` / `PipeBarrier<PIPE_ALL>()`。

### 3.3 CPU 单 launch 不可行

tikicpu 对 MIX **串行**调度：AIC 先执行 `CrossCoreWaitFlag` 会永久等待 AIV。单 launch 融合 **不作为 CPU 验收路径**。

---

## 4. SIM：标量写 GM 与 MTE 读 GM

### 4.1 现象

`y_hat`、`u_ntt`（dump 前）golden 正确，但 INTT 后 `u ≈ e₁`：Stage1 读到全零 S0。

### 4.2 机理（工程归纳）

同 kernel、同 launch 内：**Scalar 单元写 GM** 与 **MTE `DataCopy` 读 GM** 在 SIM 上 **不保证立即可见**。这与「算法算错」正交 — 须用 **UB 驻留** 或 **launch 边界** 隔离。

### 4.3 错误与正确

| 做法 | 判定 |
|------|------|
| `innerproduct_halfrows_scalar` → GM → `Process()` `DataCopy(GM→UB)` | ❌ 融合主路径（SIM 踩坑） |
| `ProcessFromScalarGm` 标量逐 lane 读 GM | ⚠️ 仅定位用；性能差 |
| `innerproduct_halfrows_to_ub` → `ProcessFromLocal` | ✅ 定案 |

---

## 5. 可复用模式

| 模式 | 适用 |
|------|------|
| **P-UB-Producer** | 任意 AIV 标量/向量段产出 → 下一段 AIV 消费：写 `LocalTensor`，`PipeBarrier<PIPE_ALL>()` |
| **P-MIX-GATE** | AIV 并行段结束 → `SET(done)`；AIC `WAIT(done)` → `SET(gate)`；AIV `WAIT(gate)` 再进 MIX 下一段 |
| **P-GM-Dump-Only** | `DataCopy(UB→GM)` 仅验收；Consumer 不得读该 GM |
| **P-Launch-Boundary** | CPU 要快路径 / SIM 要融合：拆 launch 是最简正确性保底 |

---

## 6. 案例附录

| 项 | 值 |
|----|-----|
| 探针 | `fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4` |
| 3 launch | CPU+SIM max=0 |
| 单 launch SIM | `F203_FEAS_FUSED=1`，~130s，max=0 |
| 关键 API | `innerproduct_halfrows_to_ub`、`ProcessFromLocal`、`dump_u_ntt_halfrows_ub` |
| 待扩展 | kP=5（`tr̂`）、行 2 decode、NoiseTail e₂ → v |
