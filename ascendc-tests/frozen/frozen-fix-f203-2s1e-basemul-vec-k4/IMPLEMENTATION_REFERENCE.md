# 2s1e 实现参考（详细注释索引）

**探针**：`fix-f203-2s1e-alg13-16171820-k4`  
**规范**：[`../../frozen/frozen-fix-f203-2s1e-alg13-16171820-k4/ALG13_16171820.md`](../../frozen/frozen-fix-f203-2s1e-alg13-16171820-k4/ALG13_16171820.md)  
**方法论**：[MLKEM-NTT-向量与标量实现指南.md](../../docs/notes/MLKEM-NTT-向量与标量实现指南.md)

---

## 1. 文件职责

| 文件 | 职责 |
|------|------|
| `mmad_custom.cpp` | MIX kernel：AIC MMAD + AIV Stage1/Pack/Merge/UB 融合；`mixPass` 分阶段调试 |
| `aiv_func.hpp` | `AivSplitPolyBatch`（Stage1）、`AivPackMatCPlanar`、`AivMergePlanarPoly`、平面读 mat_c |
| `2s1e_post_ntt_ub.hpp` | **`Aiv2s1eUbPipeline`**：S3→行18→ByteEncode 单 TPipe |
| `ntt_vec.hpp` | Stage3 向量 merge/mod（平面 4 行公式） |
| `hat_vec.hpp` | 行 18：向量 ê 加 + mod；**标量** `multiply_ntts_half_scalar` |
| `byte_encode12_vec.hpp` | 行 19–20 向量 ByteEncode（Gather+pack/DataCopy）；`byte_encode12_pair.hpp` 标量入口 |
| `mod_variants.hpp` | `F203_MOD_BARRETT_VEC` 等三模宏 |
| `stage1_config.hpp` | `F203_STAGE1_SPLIT` 0/1/2 |
| `stage3_config.hpp` | Stage3 mod 变体 |
| `tiling.h` | `mixPass`、`tileLength`、平面行数常量 |
| `scripts/gen_data.py` | Golden：平面 mat_c、t_hat(C)、ek/sk |
| `scripts/verify_result.py` | 对拍 + checkpoint pass 4/5 |
| `run.sh` | CPU/SIM、mixPass、Stage1 环境变量 |
| `hat_inner_product_ref.c` | golden 行 18（标量 mod） |
| `byte_encode12_ref.c` | golden ByteEncode |

---

## 2. host / GM 契约

### 2.1 `src.bin` — `[8,256] int32`

```
行 0..3 : 同一 s_poly（4 次重复，对应 4 个 ŝ）
行 4..7 : e_poly 变体（e, e+1, e+2, e+3 mod Q）
```

设备 Stage1 写 S0 时：**再次**把 s 写到 S0_ROW_S0 与 S0_ROW_S1 两套行块（逻辑 2×ŝ），ê 按 AIV 对半写入 E0/E1。

### 2.2 `mat_c` 平面 — `[96,128] int32`

行索引：`planar_row(slot, limb, half)` = `half * 48 + slot * 4 + limb`

每个 slot（0..11）8 行 = 4 limb × (lo_half, hi_half)：

- half=0：C_lo 的 hh,lh,hl,ll  
- half=1：C_hi 的 hh,lh,hl,ll  

slot 映射见 `gen_data.py` 中 `PLANAR_SLOT_*` 与 `pack_bank_planar`。

### 2.3 输出

| GM | 形状 | 含义 |
|----|------|------|
| `dst` | `[12,256]` | NTT 后 ŝ/ê（含重复 ŝ 块） |
| `t_hat` | `[4,256]` | 行 18 |
| `ek_out` | `4×384` bytes | 行 19 |
| `sk_out` | `4×384` bytes | 行 20（ŝ_hat） |

---

## 3. `mixPass` 调试矩阵（`mmad_custom.cpp`）

| mixPass | 运行阶段 | 用途 |
|---------|----------|------|
| 0 | S1+S2+S3+Hat+Encode | **生产**全链路 |
| 1 | 仅 S1 | Stage1 性能 A/B |
| 2 | 仅 S2 | MMAD |
| 3 | S3 | merge/mod |
| 4 | Hat（可从 preset 载 dst） | 行 18 |
| 5 | S1+S2+S3 | checkpoint 前半 |
| 7 | Encode only | ByteEncode 隔离 |

CPU：`g_2s1e_mix_pass`（`ASCENDC_CPU_DEBUG`）；失败时 `run.sh` 自动 5→4 用 checkpoint。

---

## 4. Stage1 向量化（`aiv_func.hpp` + `stage1_config.hpp`）

**数学**（每系数 `v in [0,Q)`）：

```
hi = v >> 6
lo = v - hi * 64    // 不是 v & 63
```

| `F203_STAGE1_SPLIT` | 实现 |
|---------------------|------|
| 0 | 标量循环 |
| 1 | bulk：`Duplicate` hi 掩码 + `ShiftRight`/`Muls`/`Sub`/`Cast` 整批 |
| 2 | 每 32 系数 tile |

环境变量：`F203_STAGE1_SPLIT`（`run.sh` 传入 SIM）。

---

## 5. Stage3 平面 merge（`ntt_vec.hpp`）

与 `gen_data.py` `merge_planar_poly` 一致：

```
raw_lo = hh*4096 + (hl+lh)*64 + ll   // 来自 half=0 四行
raw_hi = 同上 half=1
dst[0:128] = stage31_mod(raw_lo)
dst[128:256] = stage31_mod(raw_hi)
```

**无 Gather（NTT S1–S3）**：连续 `DataCopy` 读 4 行 × 128 列。行 19–20 ByteEncode 不受 NTT Gather 禁令约束。

---

## 6. 行 18 UB 融合（`2s1e_post_ntt_ub.hpp`）

流程概要：

1. S3 输出驻留 UB（或从 GM 读 dst 局部）  
2. 读 `a_hat` 批；本地 ŝ_hat / ê_hat 已在 UB  
3. `multiply_ntts_half_scalar`：逐系数 basemul（**瓶颈，故意标量**）  
4. `F203_MOD_BARRETT_VEC`：ê 加 + mod  
5. 可选写 `t_hat` GM  
6. `poly_byte_encode12_local` → ek/sk  

**禁止**：`SHAT_PEER`、对端 ŝ GM、`coef_pairs_vec`。

---

## 7. Golden 与设备解耦

| 项 | Golden | 设备 |
|----|--------|------|
| 行 18 mod | `HAT_GOLDEN_MOD_VARIANT=0` int64 | `F203_MOD_VARIANT=1` Barrett |
| Stage1 | Python 与标量路径一致 | 可向量化 |
| mat_c / dst | Python `pack_mat_c_planar` | `AivPackMatCPlanar` |

对拍：`verify_result.py` 比较 bin 文件 `max_abs_diff` 或字节相等。

---

## 8. 构建与运行

```bash
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```

关键环境变量：

- `NTTS2S1E_MIX_PASS` / `TAG5T_MIX_PASS`  
- `F203_STAGE1_SPLIT`  
- `F203_MOD_VARIANT` / `F203_STAGE3_MOD`  
- `NTTS2S1E_E_POLY_SEED` — ê 随机种子（默认 43）

---

## 9. 扩展新 MLKEM 集成时

1. 复制本目录，保持 `tiling.h` 平面常量一致  
2. 只改 `Aiv2s1eUbPipeline` 前后衔接，勿恢复竖堆 mat_c  
3. 新向量 basemul 需新数据面设计；勿抄 frozen `coef_pairs_vec`。**Gather 仅禁 NTT S1–S3**；post-NTT 用法另议。  
4. 同步改 `gen_data.py` + `verify_result.py` + 本文档一节  
