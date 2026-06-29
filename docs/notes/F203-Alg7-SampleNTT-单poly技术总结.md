# F203 Alg.7 SampleNTT 单 poly — 技术总结

**定稿**：2026-06-24  
**探针**：[`pass-fix-f203-alg7-sample-ntt-k4`](../../ascendc-tests/pass-fix-f203-alg7-sample-ntt-k4/)  
**实现方案**：[`INTEGRATION_PLAN.md`](../../ascendc-tests/pass-fix-f203-alg7-sample-ntt-k4/INTEGRATION_PLAN.md)  
**讨论**：[`qa/2026-06/2026-06-23-SampleNTT-PhaseA向量化讨论.md`](../../qa/2026-06/2026-06-23-SampleNTT-PhaseA向量化讨论.md) §13–§18；[`qa/2026-06/2026-06-24-Alg7单poly验收与R5向量compact.md`](../../qa/2026-06/2026-06-24-Alg7单poly验收与R5向量compact.md)  
**Phase A 历史（已冻结 2026-06-28）**：[`frozen-fix-f203-alg13-device-presample-a-hat-k4`](../../ascendc-tests/frozen/frozen-fix-f203-alg13-device-presample-a-hat-k4/) — 见 [`F203-Alg7-PhaseA-向量化技术总结.md`](F203-Alg7-PhaseA-向量化技术总结.md)

---

## 0. 定位

在 **单 AIV、单 poly `(j,i)`** 上实现 FIPS 203 **Alg.7 SampleNTT** 设备链至 **`â[N]`**（`N=256`，`q=3329`），I/O 与 golden 一致。  
**不是**设备蝶形 NTT；**不是** 16×4 `Â` batch（后继母探针）。  
**验收口径**：工程 POC / **I/O 等价**；非 FIPS 逐行 lazy tail、非全向量热路径。

---

## 1. 数学与输出契约

| 量 | 约定 |
|----|------|
| 输入 | `B = ρ‖byte(j)‖byte(i)`，`ρ` 由 `SEED_D` 经 Phase G 导出 |
| XOF | SHAKE128；本探针 **固定 squeeze 672B**（= 业界 504B 首批 + 1×168B tail 等价续流） |
| 候选 | `kCandPairs=224` 组 `(C0,C1,C2)` → `d1[i], d2[i]`（line 6–7） |
| rej | 交错流 `stream[448]` 上按序取 **前 256 个** `d<q` 作为 `â`（line 8–15 输出等价） |
| 截断 | `j≥256` 后 cand **不改变** 已写入的 `â` 前缀（批量 compact 与规范边扫边填等价） |

Golden：`scripts/gen_data.py` 中 `rej_scalar_from_d12` / `rej_bulk_from_d12` 互证；`test_multi_seed.py` 28 组。

---

## 2. 工程不变量

1. **单 TPipe**：SHAKE → 解交织 → d12 → rej → â 均在 UB 完成；GM 仅 seed/poly 入、**d1/d2/â** 出（xof 默认不 dump）。
2. **固定几何**：`672B` → `224 pair` → `448 stream lane`；常量由 `f203_alg7_layout.h` ↔ `scripts/alg7_geom.py` 同步。
3. **剔除语义**：`Mins(d,q)` 将 `d≥q` 标记为 `q`；compact 视 `v<q` 为接受（与 `v≠q` 在 `v≤q` 下等价）。
4. **rej 热路径**：剔除 + 交错 **禁止** `for+GetValue`（`F203_ALG7_REJ_IMPL=1/2`）；compact 当前允许标量 `GetValue`（R5 未关）。
5. **AscendC API**：写码前查 [`CANN-AscendC算子开发接口参考-查阅索引.md`](../../library/documents/CANN-AscendC算子开发接口参考-查阅索引.md)；A2 上 **`int32` 的 `Compares`/`Compare` 仅 `EQ`**，`LT` 须 `float`/`half` 链。

---

## 3. 已验证流水线（生产默认）

```text
ρ‖j‖i → SHAKE128(672B) → 标量解交织 → c0/c1/c2
  → 向量 d1/d2 → Mins×2 → Gather 交错 stream[448]
  → 标量 compact → â[256] → GM
```

| 段 | 实现 | 向量？ |
|----|------|--------|
| SHAKE | `shake_xof_kernel` UB | 库内向量/标量混合 |
| 解交织 | `DeinterleaveCandScalarFromUb` | 否（`D12_GATHER=1` 实验负优化） |
| d12 | `ComputeD12Vec` | 是 |
| 剔除 | `RejectFilterMinsUb` | 是 |
| 交错 | `Gather` + interleave ROM | 是 |
| compact | `RejScalarCompactStreamUb` | 否 |

**门禁**：R0–R4 ✅；R5 向量 compact **未关**（见 §5）。

---

## 4. 比较原语与踩坑（A2）

| 误区 | 正确做法 |
|------|----------|
| `int32` 上 `Compares(LT/NE)` | **仅 `EQ` 合法**；要 `<q` 用 `Mins` 或 Cast→`float` 再 `LT` |
| `Compares` 的 `dst` 逐字节当 0/1 | `uint8` **bit 打包**；8 个 float/int32 比较占 **1 字节** 8 bit |
| `Compares` `count` 任意长度 | Level-1 **`count`×元素大小须 256B 对齐**（如 int32 `count=128`） |
| 仿抄 `IMPL=2` 的 `Compares(LT)` on int32 | 文档违规；SIM 上可能「能跑」≠ 实机合法 |
| 未查索引就写 `GatherMask`/`GetCmpMask` | 须先索引 → PDF → **写回索引**（已入 Rule「AscendC API 查阅」） |

**剔除对照**（672B，SIM）：`Mins` **63222** ≈ `Compares+Select` **63249** ≈ 标量 **63256**（差 <0.1%）→ 默认 **`F203_ALG7_REJ_IMPL=1`**。

---

## 5. R5 向量 compact：可行性与阻塞

**语义（Host 已证）**：8-lane tile + 接受掩码（8 bit）→ 256 项 LUT（popcount + Gather 字节偏移）→ `Gather` 压紧；与标量 compact **逐系数一致**。

**尝试过的 SIM 路径（均 `a_hat` 全 0 失败）**：

1. `Compares(EQ,q)` on int32 + 首字节取反  
2. `Cast(int32→float)` + `Compares(LT)`  
3. `Compare(dst, tile, qTile, EQ)` tensor–tensor  

**判断**：**理论可行、工程未落实**；阻塞点在 SIM 上 **Compare/Compares 的 bit 掩码读出**（或 `cmpMaskUb` 布局），非 LUT/Gather 公式。生产保持 **标量 compact**；草稿见 `f203_alg7_rej_compact.hpp`（未接线）。

**后继**：单独 `ascendc-tests/` 探针只测 8-lane 掩码读法，或 `GetCmpMask`+`GatherMask` 链；勿在全链反复试错。

---

## 6. 性能（SIM tick，910B4，`blockDim=1`）

| 配置 | tick | 说明 |
|------|------|------|
| 504B XOF | ~55738 | 历史基线 |
| 672B，rej 各方案 | ~63222–63265 | +~13% 来自 **多 1×168B Keccak** |
| **672B 全链（当前）** | **~80100** | 含 SHAKE+d12+rej+GM 写回；较 ~63k 档含 **全链合一 + ROM Init 等** |

**瓶颈排序（定性）**：SHAKE 672B **>>** 标量解交织 672×GetValue ≈ compact 448×GetValue **>>** rej 向量段。

**不大改前提下的微优化（待实测）**：去掉 rej 重复 `DataCopy`；交错 ROM 改 `DataCopy` 初始化替代 448×`SetValue`；生产仅写 `â` GM。期望 **百分之几～十来个点**，**不能**单靠它们回到 55k 档。

**若 tick 为硬指标**：优先 **504B + lazy tail(168)**（约 −13%），其次母探针 batch 摊薄 XOF。

---

## 7. 验证方法论

```bash
cd ascendc-tests/pass-fix-f203-alg7-sample-ntt-k4
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
python3 scripts/test_multi_seed.py
```

- **I/O**：`d1`/`d2`/`a_hat` `max_abs_diff=0`  
- **勿**将 SIM tick 与母探针 16-poly A 的 881627/715k 混比（管线不同）  
- **完成声明**：单 poly `â[256]` 功能 **可称写好**；全向量 / FIPS 字面 / batch `Â` **不在范围**

---

## 8. 案例附录

| 路径 / 符号 | 含义 |
|-------------|------|
| `f203_alg7_d12_vec.hpp` | 全链 TPipe |
| `f203_alg7_rej_vec.hpp` | 向量 rej + **标量** compact |
| `f203_alg7_rej_filter.hpp` | `F203_ALG7_REJ_IMPL` 0/1/2 |
| `f203_alg7_rej_compact.hpp` | R5 草稿（未生产） |
| `scripts/gen_alg7_compact_lut.py` | 8-lane LUT |
| Gate R0–R4 | `INTEGRATION_PLAN.md` §5 |
| API 索引 §2026-06-23 | Compares / GetCmpMask / GatherMask |

---

## 9. 与 16-poly（Alg.13 行 3–7）的关系

**后继探针**：[`pass-fix-f203-alg13-lines3-7-a-hat-k4`](../../ascendc-tests/pass-fix-f203-alg13-lines3-7-a-hat-k4/)（方案 [`INTEGRATION_PLAN.md`](../../ascendc-tests/pass-fix-f203-alg13-lines3-7-a-hat-k4/INTEGRATION_PLAN.md)）— 16× SampleNTT 链末写 GM `a_hat[16,256]`。Phase A 全链 [`frozen-fix-f203-alg13-device-presample-a-hat-k4`](../../ascendc-tests/frozen/frozen-fix-f203-alg13-device-presample-a-hat-k4/) 中 A-v4a/b 已证伪 GM 栈半向量路线（**只读**，勿抄码）。
