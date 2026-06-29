# INTEGRATION_PLAN — Alg.13 行 3–7：设备生成 `Â`（16×`â[256]`，k=4）

**探针**：`pass-fix-f203-alg13-lines3-7-a-hat-k4`  
**状态**：**G3 PASS**（CPU/SIM `a_hat max_abs_diff=0`）；性能增量见 §5.1  
**讨论**：[`qa/2026-06/2026-06-23-SampleNTT-PhaseA向量化讨论.md`](../../qa/2026-06/2026-06-23-SampleNTT-PhaseA向量化讨论.md)、[`qa/2026-06/2026-06-24-Alg7单poly验收与R5向量compact.md`](../../qa/2026-06/2026-06-24-Alg7单poly验收与R5向量compact.md)（§7.1–§7.3）  
**上游模块（单 poly，已 PASS）**：[`pass-fix-f203-alg7-sample-ntt-k4`](../pass-fix-f203-alg7-sample-ntt-k4/)  
**下游消费**：[`pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2`](../pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/)（行 18 读 GM `a_hat`）；Phase A 全链已冻结 [`frozen-fix-f203-alg13-device-presample-a-hat-k4`](../frozen/frozen-fix-f203-alg13-device-presample-a-hat-k4/)（tick 表只读）  
**技术总结**：[`docs/notes/F203-Alg7-SampleNTT-单poly技术总结.md`](../../docs/notes/F203-Alg7-SampleNTT-单poly技术总结.md)

---

## 0. 定位与性能原则

### 0.1 算什么 / 不算什么

| 算 | 不算 |
|----|------|
| FIPS 203 **Alg.13 行 3–7**：`SEED_D` → `ρ` → **16×** Alg.7 SampleNTT → `Â` / `a_hat[16,256]` | 行 8–15（`src` / `ŝ`/`ê` 预采样） |
| 单次 **独立 launch**；输出供后续 NTT+行 18 **从 GM 读取** | 与 NTT S1–S3、行 18 **同一 kernel 融合** |
| k=4 锁定；`N=256`，`q=3329` | NTT 段 `Gather` 禁令（不适用于本 rej） |

### 0.2 任务类型（与 NTT/内积分治）

| 阶段 | 类型 | 优化杠杆 |
|------|------|----------|
| **本探针（Â 生成）** | **Keccak / SHAKE 计算密集**；d12/rej 算术极轻 | batch SHAKE；**UB 内闭环**（xof/d1/d2 不落 GM）；链末 **一次** 写 `a_hat` GM |
| NTT / 行 18（vec-k4） | 系数反复流动；**搬运敏感** | UB 驻留、tile、少读 GM（**另探针**） |

**冻结（生产默认）**：XOF **squeeze / tail / lazy while** 策略辩论——**默认仍为 672B + vec rej**（与单 poly 探针对齐）。

**实验对照（非默认）**：`F203_ALG7_XOF_504=1` 可切换 **504B**（`kTailPrefetchBlocks=0`）；见 §5.1 与 [`STATUS.md`](STATUS.md)。升为生产默认须用户拍板（~1% 种子需 tail 的 FIPS 语义；见单 poly `INTEGRATION_PLAN` §1.5）。

---

## 1. 数学与 I/O 契约

### 1.1 Alg.13 行 3–7（k=4）

对 `p, j ∈ {0,1,2,3}`（矩阵行/列，与行 18 `hat_dot_layout` 一致）：

```text
ρ ← G(d) 前 32B（d 由 SEED_D derand，k=4）
对每个 (p, j):
  â[p,j] ← SampleNTT( ρ || byte(j) || byte(p) )   // 34B 消息，Alg.7
```

共 **16** 个独立 SampleNTT；**不是**设备蝶形 NTT。

### 1.2 GM 布局（与 vec-k4 / innerproduct 唯一）

```text
flat(p, j, c) = (p * K + j) * N + c     // K=4, N=256, int32
a_hat.bin : 16 * 256 * 4 = 16384 B
```

**不变量**：与 [`hat_dot_layout.hpp`](../pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/hat_dot_layout.hpp) `a_hat_offset(p,j)` **同一公式**。

### 1.3 输入 / 输出文件（计划）

| 路径 | 形状 | 说明 |
|------|------|------|
| `input/seed_d.bin` | uint32 LE | `SEED_D` |
| `output/a_hat.bin` | int32 `[16,256]` | 设备输出 |
| `output/golden_a_hat.bin` | 同上 | `scripts/gen_data.py` |
| `output/golden_rho.bin` | 32B | 可选对拍 ρ |

调试门控（非默认）：`F203_AHAT16_DUMP_XOF=1` 可 dump `xof[16,672]`（体积大，仅 CPU 回归）。

### 1.4 验收

```bash
cd ascendc-tests/pass-fix-f203-alg13-lines3-7-a-hat-k4
python3 scripts/gen_data.py
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
python3 scripts/verify_result.py
```

- **I/O**：`a_hat` `max_abs_diff=0`（16×256 全系数）  
- **勿**与全预采样探针 881627 tick 混比（本探针 **仅** 行 3–7）

---

## 2. 几何与常量（锁定单 poly）

与 [`f203_alg7_layout.h`](../pass-fix-f203-alg7-sample-ntt-k4/f203_alg7_layout.h) / [`scripts/alg7_geom.py`](../pass-fix-f203-alg7-sample-ntt-k4/scripts/alg7_geom.py) **同值**：

| 常量 | 值（默认 672） | 504 对照（`F203_ALG7_XOF_504=1`） |
|------|----------------|----------------------------------|
| `kXofBytes` | **672** | **504** |
| `kCandPairs` | **224** | **168** |
| `kStreamLen` | **448** | **336** |
| `kSampleSeedBytes` | **34** | **34** |
| `F203_ALG7_REJ_IMPL` | **1**（vec_mins，生产默认） | 504 路径暂 **标量 rej**（168-pair 向量 interleave 待修） |

16-poly 额外常量：

| 常量 | 值 |
|------|-----|
| `kAHatPolys` | **16** |
| `kAHatBytes` | **16384** |
| `kShakeBatch` | **16**（1 AIV）或 **8**（2 AIV 各一批） |
| `kXofBatchBytes` | `16 * 672 = 10752`（默认）或 `16 * 504 = 8064`（504 对照） |

实现时建议 `f203_a_hat16_layout.h` 显式 `#include` 或数值同步单 poly `layout.h`，**禁止** silent 漂移。

---

## 3. 设备管线（默认：1 AIV，UB 全链）

### 3.1 总览

```text
SEED_D (GM)
  → BuildRhoFromSeedD → ρ[32]（标量，复用 f203_alg7_g.hpp 或 presample Phase G）
  → UB: 填 x[16,34]（16 行 ρ||byte(j)||byte(p)）
  → RunKernelShakeGeneralUb(batch=16, outLen=672) → yUb[16,672]
  → for polyIdx in 0..15:
        从 yUb 取第 polyIdx 行（仍在 UB，不整包落 GM）
        → Deinterleave → ComputeD12Vec → RejectFilterMins → Gather 交错
        → RejScalarCompactStreamUb → â_local[256]
        → DataCopy → GM a_hat[a_hat_offset(p,j)]
  → return
```

**关键**：中间 **禁止**「`yUb` 整包 DC→GM 再每 poly 读回」——Phase A 881627 档教训。

### 3.2 模块复用（从单 poly 探针）

| 模块 | 来源 | 改动 |
|------|------|------|
| `BuildRhoFromSeedD` | `f203_alg7_g.hpp` | 无 / 薄封装 |
| `FillShakeRowUb` + batch tiling | `f203_alg7_shake_xof.hpp` | `batch=16`；循环填 16 行 `(j,p)` |
| `DeinterleaveCand*` / `ComputeD12Vec` | `f203_alg7_d12_vec.hpp` | 抽 **per-poly** 函数，不绑单 poly `BuildAlg7SampleNttFromSeedD` |
| `RejectFilterDispatchUb` / `RejVecBulkFromD12Ub` | `f203_alg7_rej_vec.hpp` | 无 |
| ROM | `f203_alg7_interleave_rom.h` 等 | 几何相同可 **直接复用** |

**实现方式（建议）**：本目录 `f203_a_hat16_ub.hpp` 编排 TPipe；`#include` 单 poly 头文件（或后续抽到 `library/shared/f203_alg7/`——**本阶段可先相对路径 include**）。

### 3.3 UB 预算（910B4，1 AIV，粗算）

| 区 | 约大小 |
|----|--------|
| `xUb` 16×34 对齐 | ~0.6 KiB |
| `yUb` 16×672 | **~10.5 KiB** |
| lengths + staging | ~0.1 KiB |
| 单 poly scratch（d12+rej，复用） | ~数 KiB（见单 poly `INTEGRATION_PLAN` §3.1） |
| **合计** | **≪ 192 KiB** |

结论：**1 AIV 可一次 batch16 + 串行 16 次 UB 内 rej**，无需为 `xof` 使用 GM 中转。

### 3.4 写 GM 策略

| 项 | 策略 |
|----|------|
| `a_hat` | 每 poly 算完 **立即** `DataCopy` 到 `gm_a_hat[a_hat_offset(p,j)]`；或 16 次写同一 GM 张量不同偏移 |
| 中间 `d1/d2/xof` | **默认不写 GM** |
| 与后续内积 | 内积 launch **只读 GM** 上完整 `a_hat[16,256]`（1 或 2 AIV 均如此） |

---

## 4. 并行：2 AIV（可选，Phase 2）

**非默认**；先 **1 AIV PASS** 再开。

| blockIdx | 负责 poly | batch SHAKE | GM 写入 |
|----------|-----------|-------------|---------|
| 0 | `polyIdx` 0–7（`(p,j)` 表前 8 项） | `batch=8` | `a_hat_offset` 对应 8 块 |
| 1 | 8–15 | `batch=8` | 后 8 块 |

每 AIV 内仍是 **完整 UB 全链**（SHAKE→d12→rej→写 GM **片段**）。  
**不是**每 AIV 各生成完整 `Â`；完整矩阵靠 **GM 拼接**。

与 [`innerproduct-k4-halfrows`](../pass-fix-f203-alg11-12-innerproduct-k4-halfrows/) 的类比：**分片的是生成任务**，不是让每核握整张 `Â` 做内积。

环境：`F203_AHAT16_BLOCK_DIM=1|2`（默认 1）。

**SIM 读数**：2 AIV 在架构上应缩短**关键路径**（每核 8 poly、GM 无写冲突），但 WSL CAModel 下 **总 tick ≈ 各核周期累加**、墙钟 ~334s 与 1 AIV **持平**——**不能**用当前 tick 否定并行方案，也**不能**当作真机已验证「快一倍」。详见 §5.1「2 AIV tick vs 墙钟」；讨论见 qa §7.2。

## 5. 分阶段门禁

| 阶段 | 内容 | 通过标准 |
|------|------|----------|
| **G0** | Host `gen_data.py`：16× SampleNTT golden | 与单 poly `gen_data` 逐 poly 一致 |
| **G1** | 设备 batch16 SHAKE only | `xof` 与 golden 逐字节（调试门控） |
| **G2** | + 单 poly UB 链（polyIdx=0） | `a_hat` 首 poly PASS |
| **G3** | 16 poly 循环 + 全量 `a_hat` | CPU/SIM `max_abs_diff=0` |
| **G4** | batch16 SHAKE（`F203_AHAT16_BATCH_SHAKE=1`） | SIM PASS；tick **960098**（+31%）→ **默认关闭** |
| **G5**（可选） | `blockDim=2` 8+8 | PASS；SIM **714150**（−2.7% vs 672 默认） |
| **G6**（对照） | `F203_ALG7_XOF_504=1` 1 AIV | PASS；SIM **549224**（**−25.2%** vs **733859**）；墙钟 ~263s（−21%）；`SEED_D=20260619` 与 672 golden **bit-identical** |

### 5.1 SIM 性能对照（`Ascend910B4`，`SIM_DIRECT=1`，2026-06-24）

| 配置 | tick | 墙钟 | 对拍 | 默认 |
|------|------|------|------|------|
| 672B · 1 AIV · vec rej · 逐条 SHAKE | **733859** | ~333s | PASS | **是** |
| 504B · 1 AIV · 标量 rej · 逐条 SHAKE | **549224** | ~263s | PASS（固定种子） | 否（`F203_ALG7_XOF_504=1`） |
| 672B · 2 AIV · 8+8 | **714150** | ~334s | PASS | 否（`F203_AHAT16_BLOCK_DIM=2`） |
| batch16 SHAKE（672B 1 AIV） | **960098** | — | PASS | 否（`F203_AHAT16_BATCH_SHAKE=1`） |

**解读（504B）**：单 poly 672→504 约 **+13%** tick 收益；16 poly 全链 **−25.2%**，SHAKE/Keccak 在总 tick 中占比更高。504 对照路径少挤 **16×168B** squeeze；设备宏须经 `cmake/npu_lib.cmake` → `ascendc_compile_definitions` 传入 SIM 核。

**解读（2 AIV tick vs 墙钟）**

| 现象 | 1 AIV | 2 AIV 8+8 | 含义 |
|------|-------|-----------|------|
| SIM tick | 733859 | 714150（−2.7%） | 总 tick **几乎不变** |
| 墙钟 | ~333s | ~334s | **无并行加速痕迹** |
| 总 Keccak 次数 | 16 | 16 | 与 504B 不同：2 AIV **不减**算量，只分片 |

- **设计预期**：`KERNEL_TYPE_AIV_ONLY` + `blockDim=2`，block0/1 各跑 8×（SHAKE→d12→rej），若真机两 AIV **并行**，关键路径应接近单核一半（理想 tick/墙钟 ~50%，实际受带宽与重复 `BuildRhoFromSeedD` 影响）。
- **tick 为何几乎不动**：粗算 733859÷2 ≈ 3.67×10⁵ tick/核 × 2 核 ≈ **714k**，与实测 **714150** 吻合 → SIM 报告的 tick 更像 **各核周期累加 / 总工作量**，**不是** makespan（整次 launch 关键路径）。见 [`F203-2s1e-NTT内积UB融合技术总结.md`](../../docs/notes/F203-2s1e-NTT内积UB融合技术总结.md)（SIM Total tick ≠ 墙钟）。
- **墙钟为何不动**：`SIM_DIRECT=1` CAModel 长任务（~5min 级）下，**未表现出**两 block 并行减半；可能含仿真固定开销、多 block 调度与真机差异。**不能**据此断定真机无收益；**也不能**用当前 SIM 证明已快一倍。
- **CPU 孪生**：`f203_a_hat16_entry.cpp` 在 `ASCENDC_CPU_DEBUG && BLOCK_DIM==2` 时 **block0 内串行**跑两分片（`GetBlockIdx` 不可靠），仅影响 CPU debug，不影响 SIM 结论口径。
- **与 504B 对照**：504B 减少每 poly squeeze → tick **与**墙钟均降；2 AIV 不减 squeeze 次数 → 收益只应体现在 **并行 makespan**，而本次 SIM 两指标均未体现 → **2 AIV 保持可选非默认**；并行收益待 **真机墙钟** 或 per-core makespan profiling 验收。

**不纳入 G0–G3**：R5 向量 compact（沿用标量 compact）；**504 升为生产默认**（须 tail 语义拍板）。

---

## 6. 与周边探针关系

```text
pass-fix-f203-alg7-sample-ntt-k4   ← 模块与几何源（单 poly PASS）
        ↓ 迁入
pass-fix-f203-alg13-lines3-7-a-hat-k4              ← 本探针（仅行 3–7）
        ↓ GM a_hat[16,256]
pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2          ← 当前 host 写 a_hat；将来 vec-k4-v3 设备读
frozen-fix-f203-alg13-device-presample-a-hat-k4   ← Phase A 全链 benchmark（**已冻结**；tick 只读）
```

| 探针 | 关系 |
|------|------|
| 单 poly alg7 | **唯一合法** Alg.7 设备实现来源（非 frozen） |
| `frozen-fix-f203-alg13-device-presample-a-hat-k4` | Phase A 历史 tick；**已冻结**；本探针替代其行 3–7 专用路径 |
| `lines8-15-se-k4` | 无 `a_hat`；不冲突 |
| vec-k4-v2 | 消费方；布局必须 `hat_dot_layout` |

---

## 7. 目录与文件（计划）

```text
pass-fix-f203-alg13-lines3-7-a-hat-k4/
  INTEGRATION_PLAN.md          # 本文件
  STATUS.md
  f203_a_hat16_layout.h        # K=4, 16 poly, 与 alg7 geom 同步
  f203_a_hat16_ub.hpp          # TPipe 编排（待写）
  f203_a_hat16_entry.cpp       # __global__ 入口（待写）
  main.cpp
  CMakeLists.txt
  run.sh
  scripts/
    alg7_geom_16.py            # 复导出 + kAHatPolys（或 import 单 poly geom）
    gen_data.py
    verify_result.py
    poly_ij_table.py             # (p,j) ↔ byte(j),byte(p) 与 flat 下标
```

**禁止**：从 `frozen/` 或 Phase A 已证伪的 `f203_a_hat_rej_vec_{a,b}` 路线抄码。

---

## 8. 风险与对策

| 风险 | 对策 |
|------|------|
| `(p,j)` 与 `byte(j),byte(p)` 顺序与 golden 不一致 | G0 用 host 双重循环固定；单测 `polyIdx=0` 对齐单 poly 探针 |
| batch16 UB 上 `GetValue` 读 xof 切片 SIM 踩坑 | 优先 **行切片 DataCopy 到 poly 本地 xofUb[672]** 再解交织；参考 qa §12 |
| 复用单 poly 头文件路径耦合 | 中期可抽到 `library/shared/f203_alg7/`；首版相对 include 可接受 |
| 2 AIV GM 写冲突 | 静态 `(p,j)` 表分区，无重叠 offset |
| 与 NTT 优化混谈 | 文档与 benchmark **仅行 3–7**；内积/NTT 另表 |

---

## 9. 参考命令（实现后）

```bash
# 默认 1 AIV，672B，vec_mins，不写中间 GM
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4

# 504B 对照（非默认）
F203_ALG7_XOF_504=1 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4

# 可选 2 AIV（G5）
F203_AHAT16_BLOCK_DIM=2 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

---

## 10. 参考文档

| 资源 | 路径 |
|------|------|
| 单 poly 实现方案 | [`../pass-fix-f203-alg7-sample-ntt-k4/INTEGRATION_PLAN.md`](../pass-fix-f203-alg7-sample-ntt-k4/INTEGRATION_PLAN.md) |
| 单 poly 技术总结 | [`../../docs/notes/F203-Alg7-SampleNTT-单poly技术总结.md`](../../docs/notes/F203-Alg7-SampleNTT-单poly技术总结.md) |
| 行 18 布局 | [`../pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/hat_dot_layout.hpp`](../pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/hat_dot_layout.hpp) |
| SHAKE batch API | [`../../library/shared/shake_xof_kernel/shake_ub_helpers.hpp`](../../library/shared/shake_xof_kernel/shake_ub_helpers.hpp) |
| Phase A 历史 tick | [`../frozen/frozen-fix-f203-alg13-device-presample-a-hat-k4/SIM_BENCHMARK.md`](../frozen/frozen-fix-f203-alg13-device-presample-a-hat-k4/SIM_BENCHMARK.md) |
