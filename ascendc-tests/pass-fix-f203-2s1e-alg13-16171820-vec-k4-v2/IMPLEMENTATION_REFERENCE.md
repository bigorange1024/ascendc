# 2s1e vec-k4-v2 实现参考（详细注释索引）

**探针**：`pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2`（活跃 MLKEM 行 16–20 向量集成基线）  
**规范**：[`ALG13_16171820.md`](ALG13_16171820.md)  
**原理**：[`F203-2s1e-NTT内积UB融合技术总结.md`](../../docs/notes/F203-2s1e-NTT内积UB融合技术总结.md)  
**方法论**：[`MLKEM-NTT-向量与标量实现指南.md`](../../docs/notes/MLKEM-NTT-向量与标量实现指南.md)

源码文件头均有职责说明；本文档是**跨文件契约**与 golden 链路的总索引。

> **注释约定**：v2 工程内 `.hpp`/`.cpp`/`.c`/`.py`/`.sh` 均在文件头或关键函数处标注数据布局、
> mixPass 语义、与 golden 的对应关系；改代码时请同步更新 `IMPLEMENTATION_REFERENCE.md` §10。

---

## 1. 文件职责（v2）

| 文件 | 职责 |
|------|------|
| `mmad_custom.cpp` | MIX 入口：AIC MMAD + AIV Stage1/Pack/`Aiv2s1eUbPipeline`；`mixPass` 分阶段 |
| `main.cpp` | Host：读 input bin、launch kernel、写 output bin |
| `aiv_func.hpp` | `Aiv2s1eSplit`（S1）、`Aiv2s1ePackMatCPlanar`（S2 后 pack）、平面读 mat_c |
| `2s1e_post_ntt_ub.hpp` | **`Aiv2s1eUbPipeline`**：S3 → 行18 j→p → ByteEncode；单 slim TPipe |
| `ntt_vec.hpp` | S1 limb 拆分、S3 平面 merge + mod |
| `multiply_ntts_vec.hpp` / `alg11_*` | Alg11 向量 basemul（行 18 `compute_on_ub`） |
| `hat_dot_layout.hpp` / `hat_dot_ub_tiling.hpp` | `a_hat` 行主序、`dotScratch` 偏移 |
| `innerproduct_mod.hpp` | 行 18 final `mod_q_final_vec`（Barrett） |
| `byte_encode12_vec.hpp` | 行 19–20 向量 ByteEncode₁₂ |
| `integration_config.hpp` | `HAT_LINE18_*`、`HAT_BYTE_ENCODE`、`HAT_LINE18_FULLPOLY` |
| `tiling.h` | 平面槽位、workspace 偏移、`mixPass` |
| `scripts/gen_data.py` | 全 golden 生成（见 §2） |
| `scripts/verify_result.py` | 全链路对拍 |
| `hat_dot_halfrows_ub.hpp` | **历史**：独立瘦 TPipe dot spike；生产路径用 `stageHatDotOnly` 内联 |
| `scripts/probe_stage_verify.py` | 分阶段 head 系数快速对拍（开发期） |
| `hat_inner_product_ref.c` | 行 18 C golden（`hat_inner_product_dot` / `_add`） |
| `byte_encode12_ref.c` | ek/sk C golden |
| `alg11_rom_tables.cpp` | GM 侧 γ / Gather / interleave ROM 定义 |
| `cmake/cpu_lib.cmake` / `npu_lib.cmake` | tikicpu / ascendc 内核库与宏注入 |

---

## 2. 数据准备与 NTT golden 链

### 2.1 设计意图（与 C `MlkemNtt` 的关系）

- **设备**跑的是 Tag5T **三段式**（S1 limb → S2 int8 MMAD → S3 merge+mod），**不是**内核里逐 poly 调蝶形 `MlkemNtt()`。
- **`gen_data.py` golden** 与设备**同构**：用 Python 模拟 S0 编码、LUT 矩阵乘、平面 merge + `stage31_mod`（`mlkem_ref`），**不**在 gen_data 里逐行调用 C `MlkemNtt`。
- LUT 来自 `thirdparty/ntt_study/.../transpose_mlkem_luts_i8.h`；与 `ntt_study` 交付链同源。若用您自己的 C `MlkemNtt(FIXED_POLY)` 手算，应与 `golden.bin` 的 **slot 0**（及 slot 1–3，因 s 相同）一致。
- 行 18–20 golden 走 **独立 C ref**：`hat_inner_product_ref.c`、`byte_encode12_ref.c`。

### 2.2 `src.bin` — `[8,256] int32`

| 行 | 内容 | 4 行是否相同 | 用途 |
|----|------|--------------|------|
| `0..3` | `merged_kyber_fixed_poly.FIXED_POLY`（seed=42）×4 | **是** | 逻辑 1×s；NTT 后 ŝ[0..3] 应相同 |
| `4..7` | 基 `e`（`NTTS2S1E_E_POLY_SEED`，默认 43）→ `(e+0..3) mod Q` | **否** | 2s1e 每 AIV 2 个 ê；行 18 每 p 不同 `ê[p]` |

设备 Stage1（`Aiv2s1eSplit`）：

- **ŝ**：`src[0..3]` → 写入 S0 **两套**行块（`S0_ROW_S0` / `S0_ROW_S1`），双 AIV 各握完整 ŝ。
- **ê**：AIV0 读 `src[4..5]` → `S0_ROW_E0`；AIV1 读 `src[6..7]` → `S0_ROW_E1`。

`gen_data` 打印 `s_hat dup max_abs_diff=0`：golden `dst[4..7]`（第二份 ŝ 块）与 `dst[0..3]` 一致。

### 2.3 Golden 生成流水线（`gen_data.py`）

```text
src [8,256]
  → encode_2s1e_s0(s_poly, e_poly)     → golden_s0.bin      (Stage1)
  → mat_c_tmp_golden(s0, lut)          → 4 路 MMAD 临时
  → pack_mat_c_planar(...)             → golden_mat_c.bin   (Stage2 平面)
  → golden_dst_from_planar(mat_c)      → golden.bin         (Stage3, dst[12,256])
       merge_planar_poly + stage31_mod  (与 ntt_vec.hpp / AivMergePlanarPoly 同公式)

extract_s_e_hat(dst) → s_hat[4,256], e_hat[4,256]
a_hat [16,256] (SEED_AHAT=20260615)
  → golden_t_hat_dot.bin  (hat_inner_product_dot, 无 ê)
  → golden_t_hat.bin      (hat_inner_product_add, Σ+ê 后一次 mod)
  → golden_ek/sk_polyvec  (byte_encode12_ref)
```

### 2.4 NTT 正确性验收（`verify_result.py`）

| 检查 | 覆盖阶段 |
|------|----------|
| `S0 vs golden_s0` | Stage1 |
| `mat_c_planar vs golden` | Stage2 |
| `dst vs golden (12-poly)` | Stage3 全输出 |
| `s_hat_aiv0 vs s_hat_aiv1` | 双 AIV ŝ 复制一致 |
| `s_hat / e_hat vs golden` | 从 dst 抽取 |
| `t_hat / ek / sk` | 行 18–20（非 NTT） |

全链路 `mixPass=0` 下上述项均为 **max_abs_diff=0**（见 `STATUS.md`）。

### 2.5 与 C 参考手算对照（可选）

```text
输入：FIXED_POLY（256×int32, mod 3329）
您的 C：MlkemNtt(out, in)   // 蝶形语义
应等于：golden.bin 的 row 0（= row 1,2,3，因 s 四行相同）
设备：  output/dst.bin row 0（已对拍 golden）
```

**ê 四行不同**是探针设计，不是 NTT 错误；只测 NTT 时看 **ŝ 四行相同**即可。

---

## 3. host / GM 契约

### 3.1 `mat_c` 平面 — `[96,128] int32`

行索引：`planar_row(slot, limb, half)` = `half * 48 + slot * 4 + limb`

slot 映射（与 `tiling.h` / `dst` 对齐）：

| slot | 含义 |
|------|------|
| 0..3 | ŝ AIV0 四 poly |
| 4..7 | ŝ AIV1 四 poly（与 0..3 同值） |
| 8..9 | ê AIV0 |
| 10..11 | ê AIV1 |

### 3.2 输出 GM

| GM | 形状 | 含义 |
|----|------|------|
| `dst` | `[12,256]` | NTT 后 ŝ/ê |
| `t_hat` | `[4,256]` | 行 18 |
| `ek_out` | `4×384` B | 行 19 |
| `sk_out` | `4×384` B | 行 20（ByteEncode ŝ） |
| `a_hat` | `[16,256]` | 行 18 常量矩阵（行主序 `(p*K+j)*N+c`） |

---

## 4. `mixPass` 调试矩阵（`mmad_custom.cpp`）

| mixPass | 运行阶段 | 用途 |
|---------|----------|------|
| 0 | S1+S2+S3+Hat+Encode | **生产**全链路 |
| 1 | 仅 S1 | Stage1 |
| 2 | 仅 S2 | MMAD（可 preset s0） |
| 3 | 仅 S3 | merge/mod（可 preset mat_c） |
| 4 | Hat（+Encode 若宏开启） | 行 18；可 preset dst |
| 5 | S1+S2+S3 | CPU checkpoint 前半 |
| 7 | Encode only | preset dst+t_hat |

CPU：`g_2s1e_mix_pass`；失败时 `run.sh` 可 5→4 用 checkpoint。

---

## 5. Stage1 向量化

**数学**（每系数 `v ∈ [0,Q)`）：

```text
hi = v >> 6
lo = v - hi * 64    // 禁止 v & 63
```

| `F203_STAGE1_SPLIT` | 实现 |
|---------------------|------|
| 0 | 标量 |
| 1 | bulk 向量（默认） |
| 2 | 每 32 系数 tile |

---

## 6. Stage3 平面 merge

与 `gen_data.merge_planar_poly` / `ntt_vec.hpp` 一致：

```text
raw_lo = hh*4096 + (hl+lh)*64 + ll   // half=0 四行
raw_hi = 同上 half=1
dst[0:128]   = stage31_mod(raw_lo)
dst[128:256] = stage31_mod(raw_hi)
```

NTT S1–S3 **禁止 Gather**；ByteEncode 不受此限。

---

## 7. 行 18 UB 融合（`HAT_LINE18_FULLPOLY=1`）

默认生产路径 `stageHatDotOnly`：

```text
对每个 p：
  for j in 0..K-1:
      compute_on_ub(â[p,j], ŝ[j]) → row
      lineP += row                    // j 间不 mod（lazy Σ）
  if HAT_LINE18_DOT_ONLY=0:
      lineP += ê[p]                    // 向量 Add，仍不 mod
  mod_q_final_vec(lineP)              // 一次 final mod（Σ+ê 或仅 Σ）
```

宏：

| 宏 | 默认（全链路） | 含义 |
|----|----------------|------|
| `HAT_LINE18_DOT_ONLY` | `0` | `1`=仅 Â·ŝ |
| `HAT_BYTE_ENCODE` | `1` | 行 19–20 |
| `HAT_LINE18_FULLPOLY` | `1` | j→p `compute_on_ub`（非 legacy half） |

**不变量**：S3 后 ŝ/ê/t̂ 计算路径内驻留 UB；`a_hat` 只读 GM。

---

## 8. Golden 与设备 mod 解耦

| 项 | Golden | 设备 |
|----|--------|------|
| 行 18 final mod | `HAT_GOLDEN_MOD_VARIANT=0` int64 floor | `F203_MOD_VARIANT=1` Barrett 向量 |
| Stage3 mod | `stage31_mod`（Python） | `F203_STAGE3_MOD` 配置 |
| basemul 内部 | C ref 每步 Barrett | Alg11 向量 + ROM |

对拍比 **最终 int32 多项式**，不比中间表示。

---

## 9. 构建与运行

```bash
cd ascendc-tests/pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```

`run.sh` 默认即全链路；下表为可调项（调试时显式覆盖）：

| 变量 | 默认 | 含义 |
|------|------|------|
| `NTTS2S1E_MIX_PASS` | `0` | `mixPass` |
| `HAT_LINE18_DOT_ONLY` | `1`（run 默认）/ 全链路 `0` | dot / +ê |
| `HAT_BYTE_ENCODE` | `0`/`1` | encode |
| `BYTE_ENCODE12_PREFETCH` | `1` | ByteEncode 整 poly 路径 |
| `F203_STAGE1_SPLIT` | `1` | Stage1 向量 |
| `NTTS2S1E_E_POLY_SEED` | `43` | ê 基 poly 种子 |

SIM 全链路参考 tick：**77958**（`BYTE_ENCODE12_PREFETCH=1`）；85991（tile32 encode）；分段见 [SIM_BENCHMARK.md](SIM_BENCHMARK.md)。

---

## 10. 扩展时注意

1. 保持 `tiling.h` 平面常量与 `gen_data.py` 同步。  
2. 勿恢复竖堆 mat_c / `SHAT_PEER`。  
3. 新 basemul 数据面勿抄 frozen 路线。  
4. 改契约必同步：`gen_data.py`、`verify_result.py`、本节。
