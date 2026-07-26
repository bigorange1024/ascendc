# INTEGRATION_PLAN — Alg.7 批量向量 rej（Min+mod / 交错 / compact）

**探针**：`pass-fix-f203-alg7-sample-ntt-k4` — **单 poly** `(j,i)` 一次 Alg.7 全链 → `â[256]`（非 16×4 `Â` batch）  
**讨论**：[`qa/2026-06/2026-06-23-SampleNTT-PhaseA向量化讨论.md`](../../qa/2026-06/2026-06-23-SampleNTT-PhaseA向量化讨论.md)  
**母计划（已冻结）**：[`frozen/frozen-fix-f203-alg13-device-presample-a-hat-k4/PHASE_A_VEC_REJ_PLAN.md`](../frozen/frozen-fix-f203-alg13-device-presample-a-hat-k4/PHASE_A_VEC_REJ_PLAN.md)（A-v5 单 poly 落地 — 未阻塞集成）

---

## 0. 可行性结论

| 维度 | 判断 | 说明 |
|------|------|------|
| **功能正确性** | **高** | 批量路径：`Min` 掩码 → d1/d2 **交错** → 去 `==q` → **有序 compact** → **取前 256** 与 FIPS Alg.7 line 8–15 **输出等价**（`j<N` 由截断隐式满足；见 qa §13.5） |
| **高性能向量 rej** | **中—待证** | 掩码（`Mins`+mod）与交错（ROM `Gather`）本仓有先例；**compact** 在 A-v4a/b 曾负优化（+9%/+14%），是 SIM tick 胜负手 |
| **工程风险** | **中** | SIM 上 `Compares` 历史不稳 → 优先 `Mins`；**不做 lazy tail while**（固定 672B 预 squeeze，见 §3.5）；禁止 UB `GetValue` 主路径 |
| **建议** | **值得做 POC** | 分阶段门禁：先 golden 对拍，再谈 tick；单 poly 成功后再迁入 [`pass-fix-f203-alg13-lines3-7-a-hat-k4`](../pass-fix-f203-alg13-lines3-7-a-hat-k4/) batch=16 |

**一句话**：方案**语义可行**；能否**快于** scalar rej（881627 tick 档）取决于 **UB 内交错 + 向量 compact**，不是 `Min` 本身。

---

## 1. 目标与范围

### 1.1 算什么

FIPS 203 **Alg.7 SampleNTT** 单 poly `(j,i)` 全链至 `â[N]`，`N=256`，`q=3329`：

| 步骤 | 内容 | 本探针现状 |
|------|------|------------|
| 输入 | `B = ρ‖byte(j)‖byte(i)` | Phase G + `FillSampleSeedGm` ✓ |
| line 5 | XOF **固定 squeeze 672B**（4×rate） | `shake_xof_kernel` ✓ |
| line 6–7 | 672B → `d1[224]`, `d2[224]` | 向量 unpack ✓ |
| line 8–15 | rej → `â[256]` | **本计划** |

### 1.2 不算

- 16×4 矩阵 `Â` batch（后继母探针）
- NTT(S1–S3) 内 `Gather` 禁令**不适用**本 rej 段

### 1.3 验收

- **I/O**：`â[256]` int32 与 golden 逐系数一致（`max_abs_diff=0`）
- **命令**：`bash run.sh -r cpu -v Ascend910B4` + `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4`
- **中间量**（调试门控）：`stream[448]`、masked stream、compact 前缀可选对拍

---

## 1.4 XOF 固定 672B（工程决策，2026-06）

**为何不用 Kyber 式 `while (ctr<256) squeeze(168)`**

| 点 | 说明 |
|----|------|
| 业界首批 | `GEN_MATRIX_NBLOCKS=3` → **504B**（336 cand，期望 ~273 接受，~1% 需 tail） |
| 业界 tail | 同 absorb **每次 +168B**（1×SHAKE128 rate），非再 squeeze 504 |
| 本探针 | **一次 squeeze 672B = 504+168**，语义 = 首批 + 单次 tail 续流；统计上 448 cand 足够 |
| 向量动机 | 固定长度、满 tile、无二次 XOF 分支；多算的 168B 与多余 cand **可接受**（见 qa §13.5–§13.6） |
| 几何源 | `f203_alg7_layout.h`、`scripts/alg7_geom.py`（须同步） |

**不做**：运行时检测 `<256` 再 launch 第二次 SHAKE（母探针 batch 可另议）。

### 1.5 SIM tick：672 vs 504（实测，可回退）

| `kXofBytes` | scalar rej SIM tick | 相对 504 |
|-------------|---------------------|----------|
| 504（3×rate） | ~**55738** | 基线 |
| **672**（4×rate，**当前**） | ~**63213–63265** | **+~13%** |

**归因**：差值主要来自 **多 squeeze 168B**（Keccak 输出路径），不是「取消 tail while 省下来的」——常见种子下 504 本就够，672 是 **始终多付 1 rate 块**。

**阶段策略**

| 阶段 | XOF 策略 |
|------|----------|
| **当前原型** | **672B 固定** — 统计保险、几何固定（224 pair / 448 stream），利于 R5 向量 compact |
| **后续若追 tick** | 可 **退回 504B** + Kyber 式 `while (ctr<256) squeeze(168)`（或母探针 batch 定长，与单 poly 解耦） |
| **更激进** | 母探针全链优化时甚至可讨论 **仅首批 504、tail 按需**（须恢复 golden tail 用例）；**非**本探针当前默认 |

常量切换须同步：`f203_alg7_layout.h`、`scripts/alg7_geom.py`、ROM 脚本、`gen_data.py`。

---

## 2. 算法管线（批量 / 空间换时间）

```text
xof[672] ──► unpack ──► d1[224], d2[224]     （UB 内，kCandPairs=224）
                │
                ▼
         Mins(d1,q), Mins(d2,q)              （向量，224-wide ×2）
                │
                ▼
    mod_lane: d<q 保留，d≥q → 0（或保留 q 再 Sub）  （向量；见 §3.2）
                │
                ▼
    interleave: stream[448] = d1[i],d2[i],…   （ROM Gather，§3.3）
                │
                ▼
    compact_prefix(stream) → buf[≥256]        （先标量门禁，再向量 tile）
                │
                ▼
    â[256] = buf[0:256]                       （DataCopy GM）
```

**无 lazy tail**：XOF 已在 line5 固定 672B；rej 对全长 stream 批量掩码后 **取前 256**（`acc≥256` 停 tile）。

---

## 3. AscendC 实现要点

### 3.1 UB 布局（单 poly，建议 `scratch` ≥ 4KiB）

| 区 | 形状 | 用途 |
|----|------|------|
| `c0,c1,c2` | 各 **224** int32 | unpack 中间 |
| `d1`, `d2` | 各 **224** int32 | 掩码前后（可不写 GM） |
| `t1‖t2` | **448** int32 | interleave scratch（Alg.11 同构） |
| `stream` | **512** int32 | 交错输出（448 + pad 至 128×4） |
| `stream_m` | **512** int32 | 掩码后（拒绝=0） |
| `idxRom` | **448** int32 | interleave reorder（Init 一次） |
| `aBuf` | 256 int32 | compact 输出 |

**原则**：rej 热路径 **d1/d2/stream 不落 GM**；对拍阶段可用 `SE_ALG7_DUMP_INTERMEDIATE=1` 写 GM。

### 3.2 剔除（reject mark）：双方案 POC + 硬约束

**硬约束（rej 剔除热路径）**：`d1`/`d2` 上标记 `d≥q` 为拒绝时，**禁止** `for` 循环逐元 `GetValue`/`SetValue`。允许：
- 全长或固定 tile 的向量原语（`Mins` / `Compares`+`Select` / `DataCopy` / `Duplicate`）
- Init 阶段 ROM 拷入 UB（一次性）
- compact（§3.4）与 d12 标量解交织（默认 `F203_ALG7_D12_GATHER=0`）**另计**，不混为本节「剔除」

**语义**：接受 lane 保留 `d∈[0,q-1]`（**含合法 0**）；拒绝 lane 统一标记为 `q`（`3329`），供后续 compact 用 `v<q` 筛选。**禁止**「非零压缩」误删合法 0。

| 方案 | `F203_ALG7_REJ_IMPL` | 实现 | UB 额外 |
|------|----------------------|------|---------|
| 标量对照 | **0** | `RejScalarFromD12Ub`（GetValue） | — |
| **vec_mins（默认）** | **1** | `Mins(dst,src,q,224)` ×2 + Gather | 无 |
| **vec_mask（实验）** | **2** | `Compares(LT,q)` + `Select`；128+96 两 tile | cmpMask 128B + tile 128 int32 |

`run.sh` / CMake 默认 **`F203_ALG7_REJ_IMPL=1`**；`SE_ALG7_REJ` 字符串别名：`scalar|0`、`vec|vec_mins|1`、`vec_mask|2`。

**代码**：`f203_alg7_rej_filter.hpp`（`RejectFilterDispatchUb`）；`f203_alg7_rej_vec.hpp` 剔除后接 §3.3 交错。

**验收**：两方案 `a_hat` 与 scalar golden 一致；SIM tick 对照见 `STATUS.md`（2026-06-23：vec_mins **63222** < vec_mask **63249** < scalar **63256**，差 <0.1%）。

**CPU 孪生**：`vec_mask` 在 `ASCENDC_CPU_DEBUG` 下 dispatch 回退 `Mins`（tikicpulib 无 int32 `Compares+Select` Level-2 桩）；**tick 对比以 SIM 为准**。

**不采用**：`+q/2q` 编码（`==0` 筛拒绝已足够）；剔除段 `GetValue` 标量收尾（历史 `MaskRejectGeqQUb` 已废弃）。

### 3.3 交错：Alg.11 式 ROM + 单次 `Gather`

参照 `multiply_ntts_vec.hpp` `interleave_pairs_datacopy`：

```text
DataCopy(scratch.t1, d1, kCandPairs);
DataCopy(scratch.t2, d2, kCandPairs);
MTE2→V sync
Gather(stream, scratch.t1, idxRom, 0, kStreamLen);
```

**ROM 生成**（`scripts/gen_alg7_interleave_rom.py`，`NPAIRS=224`）：

- scratch 布局：`d1[224]‖d2[224]` 连续 int32
- 输出 `stream[2*k]` ← `d1[k]` 字节偏移 `4*k`
- 输出 `stream[2*k+1]` ← `d2[k]` 字节偏移 `896 + 4*k`
- 产物：`f203_alg7_interleave_rom.h` + 可选 `f203_alg7_interleave_rom.cpp`（`__gm__` 表）

### 3.4 Compact + 取前 256

**Gate C0（标量，必须先过）**：

```text
j = 0
for k in 0..447:
  if stream_m[k] != 0 and j < 256:
    aBuf[j++] = stream_m[k]
assert j == 256
```

**Gate C1（向量 tile，性能路径）**：

- 按 **64 pair = 128 lane** 遍历 `stream_m`（末 tile **32 pair + 96 pad** 至 128 lane）
- 每 tile：已有接受数 `acc`；本 tile 内 **prefix compact** 写入 `aBuf[acc..]`，更新 `acc`
- 向量 compact 候选（按优先级试）：
  1. **语义 LUT**（扩展 `gen_rej_uniform_table.py` 思路，索引为「tile 内接受位置」）
  2. **Gather 写回**（接受 lane 索引 → 连续 `aBuf`）
  3. 半向量：向量掩码 + **标量 compact**（仅当 C1 负优化时回退对照）

`acc ≥ 256` 后 **停止处理后续 tile**（与批量截断等价）。

### 3.5 XOF 长度（替代 lazy tail）

- **固定**：`kXofBytes = 672` = `kGenMatrixNBlocks(3) + kTailPrefetchBlocks(1)` × 168
- **Golden**：`shake128(seed).digest(672)`（同 absorb 连续 squeeze，= Kyber 首批+1×rate）
- **设备**：`FillAlg7ShakeTiling` / `RunShake128SampleNttUb` 的 `outLen=672`
- **常量同步**：`f203_alg7_layout.h`、`scripts/alg7_geom.py`、ROM 生成脚本 `NPAIRS=224`

### 3.6 分 tile 与 repeat

| 参数 | 值 |
|------|-----|
| pair / xof | **224**（672B） |
| 主 tile | 64 pair → 128 int32 lane |
| 次数 | 64 + 64 + 64 + **32** = 224 pair |
| pad | 末 tile pad 至 128 lane（哑元掩码为拒绝） |

---

## 4. 文件与模块

| 文件 | 职责 |
|------|------|
| `f203_alg7_d12_vec.hpp` | 现有 unpack；拆出 `BuildD12InUb` 供 rej 复用 |
| `f203_alg7_rej_vec.hpp` | **新建**：掩码、interleave、compact、`BuildAHatFromD12` |
| `f203_alg7_deinterleave_rom.h` | xof 解交织 Gather ROM（生成） |
| `scripts/gen_alg7_deinterleave_rom.py` | 生成解交织字节索引 |
| `f203_alg7_interleave_rom.h` | ROM 表（生成） |
| `f203_alg7_rej_scalar.hpp` | 标量 rej / compact 对照（`F203_ALG7_REJ_IMPL=0`） |
| `f203_alg7_sample_ntt_entry.cpp` | 扩展 kernel：输出 `a_hat_gm[256]` |
| `scripts/gen_alg7_interleave_rom.py` | 生成 interleave 字节索引 |
| `scripts/alg7_geom.py` | Python 几何常量（与 layout.h 同步） |
| `scripts/gen_data.py` | golden xof[672]、d1/d2[224]、a_hat |
| `scripts/verify_result.py` | 对拍 `a_hat.bin` |
| `run.sh` | `F203_ALG7_REJ_IMPL=0\|1\|2`（默认 1）、`SE_ALG7_REJ` 别名 |

---

## 5. 分阶段门禁

| Gate | 内容 | 状态 |
|------|------|------|
| **R0** | `gen_alg7_interleave_rom.py` | ✅ |
| **R1** | UB interleave Gather | ✅（rej）；**xof 解交织 Gather** ✅ Phase3 |
| **R2** | 向量剔除（`Mins` 或 `Compares+Select`；**禁止 GetValue**） | ✅ 双方案 POC |
| **R3** | 标量 compact + â 对拍 | ✅ CPU/SIM |
| **R4** | 固定 672B 预 squeeze（无 lazy tail） | ✅ |
| **R5** | 向量 compact tile | **暂停**（LUT 语义 Host 正确；SIM Compare 掩码读法未通；**生产标量 compact**） |

**禁止跳关**：未过 R3 不得声称「向量 rej 完成」。

---

## 6. Golden 参考（Python）

在 `gen_data.py` 增加：

```python
def rej_a_hat_from_d1_d2(d1, d2, q=3329) -> np.ndarray:
    """规范等价：交错流上取前 256 个 d<q。"""
    out = []
    for i in range(len(d1)):
        for d in (int(d1[i]), int(d2[i])):
            if d < q and len(out) < 256:
                out.append(d)
    if len(out) < 256:
        raise RuntimeError("672B xof insufficient — check geom constants")
    return np.array(out, dtype=np.int32)
```

`golden_stream.bin`：掩码前交错 `int32[448]`（可选中间量）。

---

## 7. 风险与对策

| 风险 | 对策 |
|------|------|
| A-v4 式负优化 | R3 标量基线；R5 仅替换 compact；保持 UB 无 GM 栈 |
| `Compares` SIM 挂 | 默认 `Mins`+`RejectGeqQ` |
| interleave 误用 n=128 ROM | 独立 `gen_alg7_interleave_rom.py` |
| 合法 0 丢失 | 拒绝标记用 `q` 或 `0` 二选一，**禁止 nonzero compact** |
| 672B 固定多付 Keccak tick | §1.5 实测 +13%；原型保留 672；追性能可回 504+tail 或母探针 batch 定长 |

---

## 8. 性能预期（诚实）

**本探针单 poly（已测）**

- 504B → 672B：**+~7500 tick（+~13%）**，归因多 squeeze 1×168B，**不**预期 tick 下降
- 672 换的是：无 tail 分支、固定 224 pair / 448 stream（向量 R5 前置）
- 追 tick 时 **允许回退 504B**（见 §1.5）；母探针另计 batch XOF

**rej 向量（R5 未完工）**
- **中性**：功能向量、compact 标量 → 可能仍慢于纯 scalar rej，但为母探针 UB 全链铺路
- **悲观**：重复 A-v4 → R3 标量 compact 暂留；生产默认 **`F203_ALG7_REJ_IMPL=1`（vec_mins）**

Host Python profile：rej ~5% CPU 时间；**设备侧**以 SIM tick 为准。

---

## 9. 后继（母探针）

单 poly PASS 后：

1. 迁入 [`pass-fix-f203-alg13-lines3-7-a-hat-k4`](../pass-fix-f203-alg13-lines3-7-a-hat-k4/)：`a_hat[16,256]`
2. batch SHAKE 已有；rej 按 poly **独立** `stream`/`â` 或 batch 16 路 UB 分块
3. 与 **715k 档** scalar 搬运基线统一 benchmark（母探针 batch XOF 可独立定长）

---

## 10. 参考

| 资源 | 路径 |
|------|------|
| d12 unpack | `f203_alg7_d12_vec.hpp` |
| interleave 范本 | `pass-fix-f203-alg11-12-multiplyntts-k4/multiply_ntts_vec.hpp` |
| A-v4 负优化 | `PHASE_A_VEC_REJ_PLAN.md` §2 |
| x86 rej | `thirdparty/liboqs/.../rej_uniform_avx2.c`（语义参考，禁止照搬控制字） |
