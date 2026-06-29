# F203 KeyGen prep 双 AIV 与 SHAKE 内嵌 — 技术总结

**读者**：在 AscendC 多 block 核内调用共享 Keccak/SHAKE 模块、或做 ML-KEM KeyGen prep 并行化的实现者  
**目的**：说明 **为何** `blockDim=2` 曾「只写一半 GM」、**如何**用内嵌语义修复、以及 **tick 为何能降 ~41%**  
**案例锚点**：`ascendc-tests/pass-fix-f203-alg13-device-keygen-k4`（G4 Step4）、`library/shared/shake_xof_kernel/`  
**讨论**：`qa/2026-06/2026-06-25-KeyGen-prep优化路线图.md` §Opt-4  
**实现方案**：`examples/incubating/exp-mlkem-f203-pke-keygen-k4/` customspec

---

## 0. 本文怎么读

| 章节 | 内容 |
|------|------|
| §1 | 数据契约：Â 16 poly、双 AIV 8+8 分片 |
| §2 | 工程不变量：内嵌子例程 vs 独立 launch 的 block 语义 |
| §3 | 故障模式与修复（ProcessInline） |
| §4 | 验证与性能解读（含 §4.1 CPU SUCCESS 误读） |
| §5 | 可复用模式 |
| §6 | 案例附录（tick、路径） |

---

## 1. 数学与数据契约

FIPS 203 Alg.13 KeyGen（ML-KEM-768，\(K=4\)）准备段：

- 输入：`SEED_D`（32 bit，探针约定 `DerandFromSeedD`）
- 一次 `G(d‖k)` → `ρ[32] ‖ σ[32]`
- 行 3–7：对 \(i,j \in \{0..3\}\)，`SampleNTT(ρ, j, i)` → `â[i,j]`，共 **16** 个 poly，每 poly **256** 系数 mod \(q=3329\)
- 行 8–15：`PRF(σ)` + `CBD_η2` → `ŝ` 多项式向量 `src[8,256]`

**GM 布局**（与 vec-k4 `a_hat_offset(p,j)` 一致）：扁平 `a_hat[4096] int32`，`offset = (p*K+j)*256`。

**双 AIV 分片**：`polyIdx 0..7` → block0，`8..15` → block1；两片 GM 区间**不交叠**，无需核间写同步。

---

## 2. 工程不变量

### 2.1 两种「多核」语义

| 模式 | `GetBlockIdx()` 含义 | 典型场景 |
|------|----------------------|----------|
| **独立 launch** | 本 kernel 的 block 编号，用于划分**本 kernel  workload** | `hat_innerproduct_halfrows`、`add_custom` |
| **内嵌子例程** | 仍是**外层 launch** 的 block 编号；子例程 UB 由**当前 AIV 独占** | `RunKernelShakeGeneralUb` 在 `BuildAHat16ShardWithUb` 循环内 |

**不变量**：内嵌调用时，子例程应处理**当前 AIV 上 UB 中的全部 batch**，等价于「该 AIV 上的 `blockIdx_local=0`、`blockNum_local=1`」。  
若子例程直接 `GetBlockIdx()` 做 batch 分片，则外层 block1 会在 `batch=1` 时**跳过全部消息**。

### 2.2 prep 单 TPipe 与 block 分工

- **Â 段**：双 AIV 并行（各 8 poly）
- **PRF+CBD 段**：仅 block0（历史 presample 单 AIV）；block1 须在末尾 `PipeBarrier<PIPE_ALL>` **等待**，不得提前 `return`

---

## 3. 故障模式与修复

### 3.1 现象

`F203_AHAT16_BLOCK_DIM=2`、SIM：

- `first_mismatch@2048`（poly 8 起为 0）
- tick ≈ 单 AIV 一半 → 仅 block0 算了 8 poly，block1 SHAKE **空转**

### 3.2 根因

`KernelShakeGeneral::Process()`：

```cpp
for (groupIdx = GetBlockIdx(); groupIdx < groupCount; groupIdx += GetBlockNum())
```

外层 `blockDim=2` 时 block1 的 `GetBlockIdx()==1`；`batch=1` → `groupCount=1` → 循环不进入 → `xof` 全 0 → rej 写出 0。

**不是** `DataCopy` 分片错误，也不是 `GetBlockIdx` 在 SIM 上「永远为 0」（那样会双写 poly 0–7，而非后半全 0）。

### 3.3 修复

`ProcessInline()`：强制 `blockIdx=0`、`blockNum=1`；`RunKernelShakeGeneralUb` 统一走内嵌路径。  
独立多核 SHAKE launch 仍用 `Process()` + `GetBlockIdx()`。

---

## 4. 验证与性能方法论

**功能**：`max_abs_diff=0` / 字节一致；须 **CPU + SIM_DIRECT** 双模式（Rule 验收阶梯）。

**性能（SIM tick，910B4）**：

| 配置 | prep tick | 说明 |
|------|-----------|------|
| 单 AIV（Opt-2 后） | ~774335 | 基线 |
| 双 AIV + ProcessInline | **454170** | **−41.4%** |
| a_hat16 独立探针 dual | **381544** | vs 单 AIV ~715k |

**解读**：

- tick 接近 **双核累加**（非墙钟减半）；Â 占 prep 大头，双 AIV 对 Â 近线性加速
- compute 段（~78k）不变；全链 852305 → **532074**
- **禁止**把 tick 改善等同于密码学语义变化；golden 仍仅验 I/O

### 4.1 CPU `[SUCCESS][AIC_x]` 与「2AIC+4AIV」误读

**现象**（CPU 孪生 / KAT 静默 log，一次全链 2 launch 后常见）：

| 段 | 典型 `[SUCCESS]` 行 | 设计语义 |
|----|---------------------|----------|
| Launch1 `f203_keygen_prep` | `AIC_0`、`AIV_0`、`AIV_1`、`AIC_1`、`AIV_2`、`AIV_3`（约 6 行） | **AIV_ONLY**，`block_dim=2` → 2 个 AIV block；**0 个 AIC 参与 prep 计算** |
| Launch2 `mmad_custom` | `AIC_0`、`AIV_0`、`AIV_1`（3 行） | **MIX**，`block_dim=1` → 1×AIC + 2×AIV |

**误读**：把上述行数相加，当成「同时占 2 AIC + 4 AIV」或「多占 AI Core 导致空转」。

**工程不变量**：

1. **`[SUCCESS][AIC_x]` / 额外套路 AIV 行** 是 **tikicpu 按芯片拓扑 spawn 子进程的 artifact**（customspec §Launch：*CPU SUCCESS 中 AIC_x 为 tikicpu 仿真伪影*），**不能**用行数推断占核数或并行度。
2. **权威剖面**（WSL 可复现）：`sim_log/profile_subtask_log*.toml` 的 `type`、`block_dim`、`core_list`、`duration` — prep 为 `type=AIV, block_dim=2`；mmad 为 `type=MIXAIC, block_dim=1, core_list=[0]`。
3. **占核契约**：两次 launch **串行**占用 **1 颗 AI Core**（非整卡多核并行 KeyGen）。
4. **实机**：占核利用率、bubble、是否 idle 须 **NPU msprof** 再验；SIM profile 已优于 CPU SUCCESS 行数。

**案例**：KAT `output/kat_liboqs_vs_ascendc.log` 每轮 seed 后均可能出现 6+3 行 SUCCESS；与 liboqs 对拍 PASS **不矛盾**。

---

## 5. 可复用模式 P-shake-inline-1

在 **多 block 外层核** 内循环调用 **共享 XOF/哈希 UB 模块** 时：

1. 模块 API 区分 `ProcessDistributed()` vs `ProcessInline()`（或显式 `forceLocalSingleBlock`）
2. 文档写明：内嵌 = 每 AIV 独立 UB batch，**不**继承外层 block 分片
3. 验收：多 block SIM 下检查 **后半 GM 是否为零**（`first_mismatch@N/2` 红旗）
4. 性能实验：先独立探针 dual PASS，再集成 prep

---

## 6. 案例附录

| 项 | 值 |
|----|-----|
| 修复提交面 | `shake_general.h`、`shake_ub_helpers.hpp` |
| prep 脚手架 | `f203_keygen_prep_ub.hpp` block0 PRF + 全体末尾 barrier |
| 生产默认 | `F203_AHAT16_BLOCK_DIM=2` |
| Opt-3 | 双缓冲流水 **已关闭**（+3.3%），无 `PIPE_SHAKE` |
| Opt-5 | Pipe 细同步 **部分合入**（CBD MTE2/V + C-04 删减）→ [F203-KeyGen-prep-Pipe细同步技术总结.md](F203-KeyGen-prep-Pipe细同步技术总结.md) |
| exp 交付 | `examples/incubating/exp-mlkem-f203-pke-keygen-k4/`（**PKE** KeyGen；与 KEM 层区分） |

**验收命令**：

```bash
cd ascendc-tests/pass-fix-f203-alg13-device-keygen-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```
