# pass-fix-f203-alg13-lines8-15-se-k4 — 设备预采样实现方案（历史名 se-vector）

**目录**：[`pass-fix-f203-alg13-lines8-15-se-k4/`](.)  
**状态**：✅ **PASS**（2026-06-23 更名 `pass-`）；含行 8–15 预采样 + [`CHAIN_NTT17.md`](CHAIN_NTT17.md) 链式 8–17。
**数据契约**：[`LAYOUT.md`](LAYOUT.md)  
**母计划**：[`DEVICE_PRF_BATCH_PLAN.md`](../pass-fix-f203-alg13-lines8-15-se-k4/INTEGRATION.md) §1b  
**标量对照**：[`fix-f203-alg13-se-device-scalar-k4`](../frozen/frozen-fix-f203-alg13-se-device-scalar-k4/)（golden PASS，178188 tick）  
**CBD 子轨**：[`pass-fix-f203-alg8-cbd-eta2-k4`](../pass-fix-f203-alg8-cbd-eta2-k4/) — **P1b-single**（`blockDim=1`，μ≈51156）  
**PRF 原语**：[`pass-shake256-ascendc-toy`](../pass-shake256-ascendc-toy/) + [`library/shared/shake_xof_kernel`](../../library/shared/shake_xof_kernel/)  
**状态**：✅ **V3 锁定**（[INTEGRATION.md](INTEGRATION.md)）；阶段二 `vec-k4-v3` 集成入口

---

## 0. 目的与边界

### 0.1 要做什么

在 **Device、单 AIV** 上完成 FIPS 203 Alg.13 **行 8–15**（$k{=}4$ 下 4×$\mathbf{s}$ + 4×$\mathbf{e}$）的 **向量化**实现：

```text
SEED_D
  → d          SHA3-256(derand_string)     [设备标量 Keccak，同 se-device-scalar]
  → σ          SHA3-512(d‖k)[32:64]        [设备标量 Keccak；σ 驻 UB]
  → 8× PRF     SHAKE256(σ‖N), N=0..7       [shake_xof_kernel, batch=8, launch blockDim=1]
  → 8× CBD     SamplePolyCBD_η=2           [alg8 P1b-single：SWAR+LUT+DataCopy+barrier]
  → src[8,256] int32  → GM
```

**与标量探针的唯一区别**：PRF 走 `shake_xof_kernel`（batch 编排）；CBD 走 alg8 **P1b-single**（非标量 `SamplePolyCbd2Row`）。Phase G（$d$/$\sigma$）保持标量 Keccak，与 [`f203_se_device_scalar.hpp`](../frozen/frozen-fix-f203-alg13-se-device-scalar-k4/f203_se_device_scalar.hpp) 一致。

### 0.2 不做什么

| 不含 | 说明 |
|------|------|
| 行 16–20 | 无 NTT、行 18 内积、ByteEncode；**本探针在 `src` 处结束** |
| Host 注入 `src` / `sigma` | Python 只写 golden；kernel **只读 `SEED_D`** |
| `Â` / Alg.16 | 超出 KeyGen 预采样段 |
| 在 `host-scalar-fullchain-k4` 上改代码 | 母探针冻结，只读 golden |
| **alg8 P2 双 AIV CBD** | 孤立探针对照；**本探针不采用**（见 §2.2） |

### 0.3 与姊妹探针分工

| 探针 | 实现 | 范围 |
|------|------|------|
| `fix-f203-alg13-se-device-scalar-k4` | 全标量串行 | 行 8–15；golden PASS（对照基线） |
| `pass-fix-f203-alg8-cbd-eta2-k4` | 向量 CBD only | 固定 `prf_out[8,128]` 输入；P1b-single = 集成 CBD 源 |
| **`pass-fix-f203-alg13-lines8-15-se-k4`** | **设备预采样**（G+P+C 向量链 + 链式 8–17） | `SEED_D` → `src`；**`blockDim=1`** |
| `pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2` | 行 16–20 向量 | 阶段二用本探针 `src` 替换 Host 注入 |

---

## 1. 密码与 golden 轨

### 1.1 统一 SHAKE256 轨

本探针与 **设备标量探针**、**alg8 golden** 一致，使用 **FIPS SHAKE256**（`rate=136`）。

不与 `host-scalar-fullchain-k4` 默认 **SHAKE128-shim** 比字节；对拍对象：

1. Python `golden_se_sampling.build_src`（`FIPS203_PRF_BACKEND=shake256`）
2. C `fips203_build_src`（同上）
3. [`fix-f203-alg13-se-device-scalar-k4`](../frozen/frozen-fix-f203-alg13-se-device-scalar-k4/) 设备输出

### 1.2 $\sigma$ 内联（不读 bin）

与 Python / C / 设备标量探针 **同式**：

```text
msg = "exp-mlkem-f203-2s1e-k4:SEED_D=" + decimal(SEED_D)
d   = SHA3-256(msg)                    # 32 B
σ   = SHA3-512(d || byte(k))[32:64]    # k=4
```

设备侧复用 [`f203_se_device_keccak.hpp`](../frozen/frozen-fix-f203-alg13-se-device-scalar-k4/f203_se_device_keccak.hpp) 的 `DerandFromSeedD` + `HashGSigma`。**$\sigma$ 生成后驻 UB**；8 路 PRF 的 33B 消息在 UB 拼 `σ[32] || N`，**不写 `sigma.bin`**。

---

## 2. 架构拍板

### 2.1 当前方案：单核 · 单 AIV（锁定）

| 决策 | 内容 |
|------|------|
| **Launch** | 全程 **`blockDim=1`**；`GetBlockIdx()!=0` 直接 return |
| **活跃核** | 仅 AIV0；AIV1 不参与预采样 |
| **向量化含义** | 单 AIV 内 **SWAR / LUT / DataCopy / shake batch 编排**；**不是**多 AIV 并行 |
| **与 NTT 段关系** | 预采样（行 8–15）与行 16–20（`vec-k4-v2`，`blockDim=2`）**分 launch**；双 AIV 留给 NTT |

**为何不用 `blockDim=8`（PRF 默认 tiling）**

- `FillShakeTiling(..., batch=8, outLen=128)` 会算出 `blockDim=8`（多 AIV 分摊 batch）
- 本探针在 Host 侧 **`FillShakeTiling` 后强制 `tiling.blockDim=1`**，由单 AIV 在 `Process()` 的 group 循环内做完 8 路 Keccak-f[1600]

### 2.2 延后项：双 AIV 生成 $s$/$e$（alg8 P2）

| 项 | 说明 |
|----|------|
| **P2 收益** | alg8 孤立探针 μ≈**28872** tick（`blockDim=2`），相对 P1b-single ~**51156** 省 ~22k |
| **为何暂不采用** | NTT 前段约定单 AIV；P2 需占双 AIV，与 `vec-k4-v2` 行 16–20 资源/launch 契约冲突 |
| **将来启用条件** | 若 **GM 搬运 + 双 launch 开销** < **单/双 AIV 生成 tick 差值**，再评估：独立 2-AIV 预采样 launch → 写满 `src[8,256]` GM → `vec-k4-v3` Stage1 从 GM 读（Stage1 布局已兼容，无需改拆分） |
| **本探针** | **不实现 P2 路径**；alg8 目录保留 P2 作对照 |

### 2.3 锁定参数一览

| 参数 | 值 | 说明 |
|------|-----|------|
| **`blockDim`** | **1** | Host launch 与 kernel 内双重保证 |
| **PRF `batch`** | **8** | tiling 内消息条数；单 AIV 串行消化 |
| **PRF launch `blockDim`** | **1**（覆写 tiling 默认） | 见 §4.2 |
| **CBD 变体** | **P1b-single** | `cbd_block_dim=1`；串行 8 行 0→7 |
| **Cube** | 不用 | 无 MatMul/NTT |

### 2.4 单 launch 数据流

```text
┌─ Kernel: f203_se_vector_k4   blockDim=1, 仅 AIV0 ────────────────────┐
│  GM in:  seed_d (uint32)                                            │
│                                                                     │
│  [Phase G] 标量 Keccak（fork se-device-scalar）                      │
│      DerandFromSeedD → d[32] → HashGSigma → σ[32]  (UB)            │
│                                                                     │
│  [Phase P] shake_xof_kernel  batch=8, launch blockDim=1             │
│      UB: msg[8,33] = σ‖N  →  prf_out[8,128] (UB；V1 可 dump GM)   │
│                                                                     │
│  [Phase C] alg8 P1b-single（单 AIV，8 行串行）                       │
│      SWAR + LUT + DataCopy + PipeBarrier → src_gm[8,256]             │
│                                                                     │
│  GM out: src.bin（与 vec-k4-v2 src.bin 同布局）                     │
└─────────────────────────────────────────────────────────────────────┘
         │
         │  阶段二 vec-k4-v3：独立 launch 或同核分段
         ▼
┌─ vec-k4-v2 Stage1  blockDim=2 ──────────────────────────────────────┐
│  两 AIV 均从 GM src[0..3] 读 ŝ；ê 按行 4–5 / 6–7 分片读            │
└─────────────────────────────────────────────────────────────────────┘
```

**集成契约**：本探针输出 `src` 写 GM 即足够；Stage1 **不**要求 CBD 与 NTT 同 launch、**不**要求 UB 跨 AIV 递 poly。

---

## 3. 实现方法（fork 单用例壳）

### 3.1 总体策略

**一次只叠一层**（同 alg8 门控工作法）：每步 CPU/SIM PASS + tick 记录 → 再进下一步。

**基座 fork 顺序**：

1. **壳**：复制 [`fix-f203-alg13-se-device-scalar-k4`](../frozen/frozen-fix-f203-alg13-se-device-scalar-k4/) 的 `main.cpp`、`data_utils.h`、`run.sh`、`CMakeLists.txt`、`gen_data.py`、`verify_result.py` 骨架
2. **Phase G**：原样 include `f203_se_device_keccak.hpp` + `DerandFromSeedD` / `HashGSigma`（或从 scalar 头拆出，不重复实现 Keccak）
3. **Phase P**：从 [`pass-shake256-ascendc-toy`](../pass-shake256-ascendc-toy/) 迁入 `FillShakeTiling` + `shake_xof_kernel` 接线（见 §4.2）
4. **Phase C**：从 alg8 **include / 软链** `f203_cbd_eta2.hpp`、`f203_cbd_eta2_sw_lut.hpp`、`f203_cbd_eta2_ub_io.hpp`、`cbd2_ab_lut.h`；编译 `-Dcbd_block_dim=1`（等同 `CBD_VARIANT=P1b-single`）

**禁止**：从零写 CBD/PRF；未 PASS 就同时上 batch PRF + P1b-single + 去 barrier。

### 3.2 与标量探针对照（改什么、不改什么）

| 段 | se-device-scalar（现状） | se-vector（目标） |
|----|--------------------------|-------------------|
| 入口 | `f203_se_device_k4` | `f203_se_vector_k4` |
| `blockDim` | 1 | 1（不变） |
| Phase G | `DerandFromSeedD` + `HashGSigma` | **同代码** |
| Phase P | 标量 `PrfShake256` ×8 循环 | **`shake_xof_kernel`，batch=8，`tiling.blockDim=1`** |
| Phase C | 标量 `SamplePolyCbd2Row` | **V2**：仍用标量 CBD 验链；**V3**：`F203CbdEta2::SamplePolyCbd2Batch8`（P1b-single） |
| GM I/O | 仅 `seed_d` in / `src` out | **同契约**（[`LAYOUT.md`](LAYOUT.md)） |
| workspace/tiling | 占位 64B | PRF 段需真实 shake tiling + workspace（照 shake256-toy） |

### 3.3 待建文件

| 文件 | 职责 |
|------|------|
| `f203_se_vector_entry.cpp` | `__global__` 入口；`GetBlockIdx()==0`；调度 G → P → C |
| `f203_se_vector.hpp` | 阶段编排、UB 布局、`PipeBarrier` 边界 |
| `f203_se_vector_prf.hpp` | σ→`msg[8,33]` 填充；`FillShakeTiling` + 强制 `blockDim=1`；调用 `shake_xof_kernel` |
| `f203_se_device_keccak.hpp` | 从 scalar 探针 copy（Phase G） |
| `main.cpp` / `data_utils.h` | Host launch；`kBlockDim=1`；PRF tiling/workspace 分配 |
| `gen_data.py` | `seed_d.bin` + `golden_src.bin` + **V1** `golden_prf_out.bin` |
| `verify_result.py` | `src` vs golden；V1 可选 `prf_out` |
| `run.sh` / `CMakeLists.txt` | 链接 `shake_xof_kernel` + alg8 CBD 头；`-Dcbd_block_dim=1` |
| `SIM_BENCHMARK.md` | 门控 tick 表（待建） |

### 3.4 刻意不引入

- `vec-k4-v2` 的 NTT / 2s1e pipeline 头文件  
- alg8 **P2** 的 `RowForBlock` / `cbd_block_dim=2`  
- Host `fips203_build_src` 在 launch 路径（仅 verify 侧 C ref）

---

## 4. 各 Phase 实现要点

### 4.1 Phase G — $\sigma$ 生成（标量，不变）

- 仅 `GetBlockIdx()==0` 执行
- 逻辑与 [`BuildSrcFromSeedD`](../frozen/frozen-fix-f203-alg13-se-device-scalar-k4/f203_se_device_scalar.hpp) 前两步相同：`DerandFromSeedD` → `HashGSigma`
- `σ[32]` 驻 UB；`PipeBarrier<PIPE_ALL>` 后进入 PRF

### 4.2 Phase P — 8× SHAKE256 PRF（单 AIV batch）

**Host 侧**（照 `pass-shake256-ascendc-toy/main.cpp`）：

```cpp
FillShakeTiling(&tilingHost, /*batch=*/8, /*maxMsgLen=*/33, /*outLen=*/128, SHAKE256_RATE_BYTES);
tilingHost.blockDim = 1;   // 覆盖默认 8
```

**Device 侧**：

- 在 UB 构造 8×33B：`msg[i] = σ || byte(i)`，`i=0..7`
- 调用 `shake_xof_kernel::Process()`；单 AIV 按 `groupIdx += realBlockNum` 做完 8 路
- 输出 `prf_out[8,128]`：V1 写 GM 供 `verify_result.py`；V3 前可仍走 GM

**V1 验收**：`prf_out` vs `golden_prf_out.bin`；**暂不跑 CBD**。

### 4.3 Phase C — CBD（alg8 P1b-single）

**V2（标量 CBD，验全链）**：

- 从 se-device-scalar 复制 `SamplePolyCbd2Row`；PRF 输出后逐行标量 CBD → `src`
- 目的：在换向量 CBD 前，确认 **G+P 向量 PRF 路径** 与 golden 一致

**V3（向量 CBD）**：

- 调用 `F203CbdEta2::SamplePolyCbd2Batch8(prf_gm_or_ub, src_gm)`，与 [`f203_cbd_eta2_entry.cpp`](../pass-fix-f203-alg8-cbd-eta2-k4/f203_cbd_eta2_entry.cpp) 相同 API
- **`blockDim=1`**；串行 8 行；**必须** `DataCopy` + `PipeBarrier<PIPE_ALL>`（alg8 已证无 barrier → verify FAIL）
- **不**引入 P2 的 `RowForBlock`

**V4（PRF→CBD UB 直传，可选）**：

- alg8 P3 思路：PRF 结果留在 UB，`SamplePolyCbd2Batch8` 读 UB 而非 GM `prf_out`
- 省 PRF 写 GM + CBD 读 GM 的往返；tick 目标再压 ~10–20k

### 4.4 Pipe 与同步

| 边界 | 建议 |
|------|------|
| G → P | `PipeBarrier<PIPE_ALL>` |
| P → C | MTE + barrier（V4 前若经 GM，同 alg8 P1b） |
| 行与行 CBD | 单 AIV 内顺序；P1b-single 每行 3× barrier |

首期不深挖 pipe 重叠；**正确性 PASS 优先**。

---

## 5. 门控实现顺序

与 alg8 [`CBD_ETA2_OPTIM_PLAN.md`](../pass-fix-f203-alg8-cbd-eta2-k4/CBD_ETA2_OPTIM_PLAN.md) 同工作法。

| 步骤 | 内容 | 验收 | tick 粗估（`blockDim=1`） |
|------|------|------|---------------------------|
| **V1** | fork scalar 壳 + `gen_data` + `verify`；**Phase G + P** | `prf_out` vs golden | ~80k–90k |
| **V2** | V1 + **Phase C 标量 CBD**（se-device 公式）→ `src` | `src` max_abs_diff=0 | ~150k–170k |
| **V3** | V2 CBD 换 **alg8 P1b-single** | 同上 | 目标 **<120k**（G+P ~85k + CBD ~51k） |
| **V4** | PRF out **UB→CBD** 零 GM（alg8 P3 思路） | 同上 + tick 下降 | 视实测 |

**禁止**：未 PASS 就同时上 batch PRF + P1b-single + 去 barrier；未 PASS 就改 `vec-k4-v2`。

每步写入 [`SIM_BENCHMARK.md`](SIM_BENCHMARK.md) 与 [`STATUS.md`](STATUS.md)。

### 5.1 V1 落地清单（下一步）

```bash
# 1. fork 目录骨架（从 se-device-scalar-k4）
# 2. CMake：加入 shake_xof_kernel 源 + include path
# 3. f203_se_vector_prf.hpp：σ→msg[8,33] + shake Process，tiling.blockDim=1
# 4. gen_data.py：增加 golden_prf_out.bin（SHAKE256，同 SEED_D）
# 5. verify_result.py：--prf-only 或分阶段开关
# 6. run.sh -r cpu/sim -v Ascend910B4
```

---

## 6. 验收标准

```bash
cd ascendc-tests/pass-fix-f203-alg13-lines8-15-se-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```

| 检查项 | 标准 |
|--------|------|
| `src` vs Python `golden_src.bin` | `max_abs_diff=0` |
| `src` vs C `fips203_build_src`（SHAKE256） | `max_abs_diff=0` |
| vs `se-device-scalar-k4` | 同 `SEED_D` 系数一致 |
| SIM tick | 记入 `STATUS.md` / `SIM_BENCHMARK.md` |
| 行 16–20 | **不跑**、不验 |

**PASS**：目录 **`pass-fix-f203-alg13-lines8-15-se-k4`**（2026-06-23）；阶段二 **`vec-k4-v3`** 待开工（母计划 §2）。

---

## 7. 性能预算（SIM tick，Ascend910B4）

**权威分段表**：[`SIM_BENCHMARK.md`](SIM_BENCHMARK.md)（标量/向量按 G、P、C 拆分；标明门控/差分/孤立探针测法）。

| 段 | 实现类型 | tick（V3 实测/差分） | 测法 |
|----|----------|----------------------|------|
| G（d+σ） | **标量** | ~3–8k（粗估） | 未单独门控 |
| P（8× PRF） | **向量** | **83478**（含 G） | V1 门控 |
| C（8× CBD） | **向量** P1b | **49675** | V3−V1 差分 |
| **V3 合计** | 标量 G + 向量 P + 向量 C | **133153** | V3 门控 |
| 全标量对照 | se-device-scalar-k4 | **178188** | 姊妹探针 |

**注意**：alg8 孤立 P1b ~51156 与集成 C 段 ~49675 不可直接等同；不可用 `shake256-toy` 单路 ×8 代替 batch PRF tick。

---

## 8. 风险与反模式

| 反模式 | 后果 |
|--------|------|
| 读 `sigma.bin` / `src.bin` 作输入 | 偏离集成契约 |
| 用 `blockDim=8`/`2` 或多 AIV 跑本段 | 违背单核单 AIV 拍板 |
| 默认 `FillShakeTiling` 的 `blockDim=8` 不覆写 | 8 AIV 分摊 PRF，与契约冲突 |
| 直接上 alg8 P2 追 tick | 与 NTT 前单 AIV 策略冲突 |
| 8× 独立 launch PRF | launch 开销大；应用 `batch=8` + `blockDim=1` |
| 去掉 CBD `PipeBarrier` 追 tick | alg8 已证 verify FAIL |
| 在本目录接 NTT | 范围膨胀；应放 `vec-k4-v3` |
| 与 SHAKE128-shim golden 对拍 | 轨不一致，误报 FAIL |

---

## 9. 文档与 INDEX 登记

实现启动后同步：

- [`DEVICE_PRF_BATCH_PLAN.md`](../pass-fix-f203-alg13-lines8-15-se-k4/INTEGRATION.md) §7 进度表  
- [`ascendc-tests/INDEX.md`](../INDEX.md)  
- [`qa/TODO.md`](../../qa/TODO.md) T13a-v  
- alg8 计划 §11「回填」指向本目录

---

## 10. 下一步

1. ~~架构~~ → **单 AIV 锁定**；P2 双 AIV **延后**（§2.2）  
2. ~~门控~~ → **V1 = Phase G + P**；V2 标量 CBD 验链；V3 P1b-single  
3. ~~**实现 V1**~~ → ✅ PASS（SIM **83478** tick）
4. ~~**V2 标量 CBD**~~ → ✅ PASS（SIM **193441** tick）
5. ~~**V3 P1b-single**~~ → ✅ PASS（SIM **133153**）；**默认路径**
6. **V4** 实验：bulk UB CBD PASS（**158901** tick，慢于 V3）；**不接入集成**
7. **链式探针（行 8–17）** → ✅ PASS — [`CHAIN_NTT17.md`](CHAIN_NTT17.md)；CPU 同 GM + SIM 分阶段；SIM **~177553** tick
8. **阶段二** → fork `vec-k4-v3`；契约见 [`INTEGRATION.md`](INTEGRATION.md)
