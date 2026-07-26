# Alg.11/12 向量化实现方案（pass-fix-f203-alg11-12-multiplyntts-k4）

**更新**：2026-06-16（晚：`ALG11_MEM_OPS=1` 定稿）

**当前默认（SIM 最快已验证组合）**：

| 宏 | 默认 | 含义 |
|----|------|------|
| `ALG11_IMPL` | `1` | 向量 Alg.12 |
| `ALG11_VEC_VARIANT` | `2` | B2：Gather deinterleave |
| `ALG11_VEC_OPTS` | **`0`** | legacy Barrett（含负值修正） |
| `ALG11_MEM_OPS` | **`1`** | **`__gm__` ROM + Init `DataCopy`**（见 §0） |

```bash
bash run.sh -r cpu -v Ascend910B4                         # 默认 MEM_OPS=1
bash run.sh -r sim -v Ascend910B4
bash scripts/ab_mem_ops.sh sim                          # MEM_OPS 1 vs 0
ALG11_VEC_OPTS=1 bash run.sh -r sim -v Ascend910B4      # §6 微优化（较慢，勿与 MEM_OPS 混为一谈）
ALG11_MEM_OPS=0 bash run.sh -r sim -v Ascend910B4       # SetValue/CreateVecIndex 对照
ALG11_VEC_VARIANT=1 bash run.sh -r sim -v Ascend910B4  # B1（同样走 ROM 索引）
ALG11_IMPL=0 bash run.sh -r cpu -v Ascend910B4           # 标量 C
```

**参考**：已冻结 [`frozen-fix-f203-2s1e-basemul-vec-k4`](../frozen/frozen-fix-f203-2s1e-basemul-vec-k4/)（勿抄）；本探针为 **单次 MultiplyNTTs(f, g)** 权威实现。  
**日纪要**：[qa/2026-06/2026-06-16-Alg11-12向量化与微优化A-B.md](../../qa/2026-06/2026-06-16-Alg11-12向量化与微优化A-B.md)

---

## 0. 为何本版性能最好（2026-06-16 结论）

### 0.1 推荐栈

```text
B2 Gather  deinterleave（4×128 宽 Gather）
  + 全向量 Barrett ×3（ALG11_VEC_OPTS=0）
  + γ / Gather 索引 / interleave reorder：__gm__ 编译期 ROM
  + Kernel Init 一次 DataCopy(GM→UB)，Compute 热路径零 SetValue/GetValue
```

### 0.2 SIM 数据（Ascend910B4，单次 kernel）

| 配置 | Model RUN TIME | Total tick | 相对最快 |
|------|----------------|------------|----------|
| **`MEM_OPS=1`（默认）** | **~1324 ms** | **~9031** | 基准 |
| `MEM_OPS=0`（CreateVecIndex + 标量 interleave） | ~2312 ms | ~16359 | **+81% tick** |
| 同日早 `VEC_OPTS=0` 无 ROM（CreateVecIndex 每 tile） | ~2020 ms | — | 仍慢于 MEM_OPS=1 |
| `VEC_OPTS=1`（标量 `SetValue` 填 Gather 索引） | ~2184 ms | — | 最劣微路径之一 |

**要点**：把 γ 从 Compute 挪到 Init ** alone 几乎不降 tick**（单 tile 仍 128 次写 UB）；真正收益来自 **消灭热路径上所有标量 UB 访问**（索引、interleave、γ 填表）。

### 0.3 机制（为何 DataCopy 赢 SetValue / CreateVecIndex）

| 环节 | 慢路径（`MEM_OPS=0`） | 快路径（`MEM_OPS=1`） |
|------|----------------------|----------------------|
| γ LUT | Init/Compute 标量 `SetValue`×128 | Init **1×** `DataCopy(128)` from `gAlg11GammasGm` |
| Gather 字节索引 | 每 tile `CreateVecIndex`+`Muls`+`Adds`（3 条向量）或 `SetValue`×256 | Init **2×** `DataCopy(128)`；Compute 只读 UB |
| interleave | 标量 `SetValue`×256 | `DataCopy(c0→t1,c1→t2)` + **1×** `Gather(h, t1‖t2, reorderRom, 256)` |
| Alg.12 主核 | 相同（向量 Mul + Barrett） | 相同 |

PEM/SIM 上 **MTE2 突发搬运** 远便宜于 **标量 LD/ST 逐元素写 UB**（与 ByteEncode、basemul spike 经验一致）。

### 0.4 实现要点（踩坑）

1. **ROM 放 `__gm__`**：`alg11_rom_tables.cpp` 由 `multiply_ntts_kernel.cpp` `#include`，保证 SIM 链接可见。
2. **interleave scratch 须物理连续**：用 ws 内相邻 `t1‖t2`（各 128 int32），**不能**用两个独立 `TQue` 缓冲区拼 reorder（字节偏移 512 假设不成立）。
3. **Init 后 `PipeBarrier<PIPE_MTE2>`**：`copy_rom_int32_ub` 之后、Compute 读 UB 之前（见 `alg11_vec_pipe.hpp`）。
4. **AoS 解交织仍用 Gather**：`DataCopy+SliceInfo` 无法表达 stride-2 偶/奇列（见 [DataCopy 知识库 §6](../../docs/notes/ascendc-DataCopy与数据搬运知识库.md)）；索引预计算走 ROM，不走路径内填表。
5. **`ALG11_VEC_OPTS=1` 默认仍关闭**：其「固定 n 索引」用 **标量 SetValue** 替代 CreateVecIndex，与 MEM_OPS 目标相反；迁入 basemul 时单独 A/B basemul 无负值修正即可。

### 0.5 迁入 basemul 清单

- 复制 **`RomUbLuts` + `init_rom_luts_ub` + `alg11_rom_tables`** 模式到 [`fix-f203-2s1e-alg13-16171820-k4`](../frozen/frozen-fix-f203-2s1e-alg13-16171820-k4/) 行 18（**勿** fork 已冻结 basemul-vec）。
- 行 18 K 循环：`BUFFER_NUM=2` 时 ROM Init 仍 **每核一次**；勿在 **每 tile / 每 pair** 重填 LUT。
- 真卡 OPProf 复核 tick 排序（SIM 已验证方向，真机待测）。

---

## 1. 目标

在 **Add-custom 外壳**（`multiply_ntts_kernel.cpp`：pipe / que / DataCopy）不变的前提下：

1. AoS → **4 条 SoA lane**（`a0,a1,b0,b1`）；
2. lane 上 **element-wise** `Mul` / `Add`（`pairCount=128`）；
3. 每步乘法后 **BarrettReduce**（Alg.12，**3 次**，不能 lazy）；
4. `c0,c1` **交织**写回 `h`（`MEM_OPS=1`：`DataCopy`+`Gather`；`0`：标量 `SetValue`）。

玩具：`N=256`，`q=3329`；`f̂[i]=(17i+3)%q`，`ĝ[i]=(13i+7)%q`；`γ[i]=kAlg11Gammas[i]`；golden 由 `alg11_12_ref.h` / Python 生成。

---

## 2. 数据布局

### AoS（CopyIn 后 `fLocal` / `gLocal`）

| 下标 | 含义 |
|------|------|
| `f[2i]` / `g[2i]` | `a0` / `b0` |
| `f[2i+1]` / `g[2i+1]` | `a1` / `b1` |

### SoA 四 lane（各长 128）

| LocalTensor | 内容 |
|-------------|------|
| `a0` | `[f[0], f[2], …]` |
| `a1` | `[f[1], f[3], …]` |
| `b0` | `[g[0], g[2], …]` |
| `b1` | `[g[1], g[3], …]` |

**只 deinterleave 一次**；禁止在每个 `Mul` 前重复拆 pair。

---

## 3. Alg.12 向量语义

```
t1 ← Barrett(a1 ⊙ b1)
t2 ← t1 ⊙ gammaV
c0 ← Barrett(a0 ⊙ b0 + t2)
c1 ← Barrett(a0 ⊙ b1 + a1 ⊙ b0)
h  ← interleave(c0, c1)
```

与 `alg11_12_ref.h` / FIPS Alg.12 一致。3 次 `reduce` 在 `γ` 一般时不可省（先 `reduce(a1·b1)` 再乘 `γ` 防 int32 溢出，见日纪要）。

---

## 4. 实现落点

| 文件 | 作用 |
|------|------|
| `multiply_ntts_kernel.cpp` | Add 外壳；`wsQue`（8×128）+ Init ROM→UB（γ / Gather 索引 / interleave reorder） |
| `multiply_ntts_ub.hpp` | `ALG11_IMPL=0` 标量 C；`1` → `compute_on_ub(..., wsBase, rom)` |
| `multiply_ntts_vec.hpp` | B1/B2、`alg12_elementwise_vec`；`ALG11_MEM_OPS` / `ALG11_VEC_OPTS` 分支 |
| `alg11_rom_tables.cpp` | `__gm__` γ / Gather 字节索引 / interleave reorder（kernel `#include`） |
| `alg11_ub_load.hpp` | `copy_rom_int32_ub`：`DataCopy(GM→UB)` |
| `alg11_fixed_n256.hpp` | `ALG11_VEC_OPTS=1` 且 `MEM_OPS=0` 时 Gather 索引 `SetValue` |
| `multiply_ntts_config.hpp` | `ALG11_IMPL` / `ALG11_VEC_VARIANT` / `ALG11_VEC_OPTS` / **`ALG11_MEM_OPS`** |
| `alg11_12_ref.h` / `.c` | host golden |
| `alg11_gammas.h` | FIPS `kMlkemGammas[128]` |

### UB 工作区（`ALG11_MEM_OPS=1`）

`wsQue`：`a0,a1,b0,b1,c0,c1,t1,t2` — `kVecWsInts = 8×128`。  
Init 独立 UB（各 `DataCopy` 一次自 `__gm__` ROM）：

| 队列 | 长度 | 用途 |
|------|------|------|
| `gammaLutQue` | 128 | Alg.12 `Mul(·,γ)` |
| `gatherEven/OddQue` | 128 | B1/B2 deinterleave `Gather` 字节索引 |
| `interleaveReorderQue` | 256 | 出口 `Gather` reorder（scratch=`t1‖t2`） |

Compute 热路径：**无** `SetValue`/`GetValue`/`CreateVecIndex`。

### Barrett（默认 `ALG11_VEC_OPTS=0`）

**全向量 Barrett** + `wrap_mod_vec_runtime` final clamp：

1. 负值修正：`dst += (dst>>31)*(-q)`（legacy 默认保留）
2. 两步 `Muls`/`ShiftRight`/`Sub`（μ=78,k=18；μ=5039,k=24）
3. `wrap_mod_vec_runtime`

`reduce_zq_vec_barrett_dispatch` 在 `ALG11_VEC_OPTS=1` 时走 `reduce_zq_vec_barrett_basemul`（省略步骤 1，FIPS 行 18 输入 `[0,q)`）。

---

## 5. Gather（B2）与 interleave

1. **deinterleave**（`MEM_OPS=1`）：Init `DataCopy` 预置 `8i`/`8i+4` 字节索引 → 4× `Gather`（128 宽）。
2. **deinterleave**（`MEM_OPS=0` legacy）：`CreateVecIndex`+`Muls`+`Adds` 或 `SetValue` 填表。
3. **interleave**（`MEM_OPS=1`）：`DataCopy(c0→t1, c1→t2)` + `Gather(h, t1, reorderRom)`；scratch 为 ws 内连续 `t1‖t2`。
4. **interleave**（`MEM_OPS=0`）：128×2 标量 `SetValue`。
5. **阶段合并**：deinterleave 仅前期一次；`alg12_elementwise_vec` 无 Gather。

---

## 6. 微优化（`ALG11_VEC_OPTS=1`，非默认）

| # | 内容 | FIPS 203 依据 | legacy (`OPTS=0`) |
|---|------|---------------|-------------------|
| 1 | 固定 n Gather 索引填表 | `n=256` 字节偏移 `8i`/`8i+4` | `CreateVecIndex`+`Muls`+`Adds` |
| 2 | `Duplicate(γ)` | 行 18 `kMlkemGammas`（迁入时用 `DataCopy`） | 标量 `SetValue(1)` |
| 3 | `reduce_zq_vec_barrett_basemul` | NTT 域非负 | 含负值修正 Barrett |

三项 **CPU/SIM golden 均 ✓**；**默认关闭**，因 SIM 墙钟劣于 legacy。

---

## 7. SIM A/B

### 7.1 `ALG11_VEC_OPTS`（2026-06-16，B2）

| `ALG11_VEC_OPTS` | CaModel Model RUN TIME | verify |
|------------------|------------------------|--------|
| **0** legacy（**默认**） | **2019.86 ms** | ✓ |
| **1** §6 微优化 | 2183.52 ms（约 **+8.1%**） | ✓ |

### 7.2 `ALG11_MEM_OPS`（2026-06-16，B2，`OPTS=0`）

| `ALG11_MEM_OPS` | Model RUN TIME | Total tick | verify |
|-----------------|----------------|------------|--------|
| **1** `__gm__`+`DataCopy`（**默认**） | **~1324 ms** | **~9031** | ✓ |
| **0** SetValue/CreateVecIndex | ~2312 ms | ~16359 | ✓ |

**结论**：固定 LUT/索引应 **`__gm__` ROM + Init `DataCopy`**；标量 `SetValue` 填表在 PEM 上约 **+45% tick**。interleave 用 `DataCopy(t1‖t2)`+`Gather` 替代 256×`SetValue`。

```bash
bash scripts/ab_mem_ops.sh sim   # MEM_OPS 1 vs 0
```

---

## 8. 可选优化（记录，未实验）

面向 **行 18 全链路**（[`fix-f203-2s1e-alg13-16171820-k4`](../frozen/frozen-fix-f203-2s1e-alg13-16171820-k4/) / `Aiv2s1eUbPipeline`）。

| 项 | FIPS 适用 | 说明 |
|----|-----------|------|
| 双缓冲 `BUFFER_NUM=2` | ✓ | K 次 basemul 循环重叠 CopyIn/Compute/CopyOut |
| 2 AIV 任务并行 | ✓ | 按 `p`/slot 分片，非 128 pair 对半 |
| 减 Barrett 次数 | ✗ | `γ` 一般时需 3 次 |
| Cast/Div 替代 Barrett | ✗ | `2q²` > 2²⁴ |
| Scatter interleave | ✗ | A2 无 Scatter |
| 上游 SoA GM | ✗ | 内积仅 AoS 多项式 |

---

## 9. 验收

| 项 | CPU | SIM |
|----|-----|-----|
| `ALG11_IMPL=0` | ✓ | ✓ |
| `IMPL=1` B1 | ✓ | ✓ |
| `IMPL=1` B2（默认 `OPTS=0`） | ✓ | ✓ |
| `IMPL=1` B2 `OPTS=1` | ✓ | ✓ |
| 公式 vs `hat_basemul` | 同序 | — |

---

## 10. 后续

- 以 **`ALG11_MEM_OPS=1` + `VEC_OPTS=0` + B2** 为 basemul 迁入基线（见 §0.5）。
- 真卡 OPProf：`reduce_zq_vec_barrett_basemul`（`OPTS=1` 第 3 项）单独 A/B，勿与 ROM 索引混测。
- §8 双缓冲 / 行 18 墙钟 profiling。
