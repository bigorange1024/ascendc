# fix-f203-2s1e-alg13-16171820-vec-k4 — 行 16–20 向量集成方案

**更新**：2026-06-18（增补内积单用例对接）

## 目的

在 [`fix-f203-2s1e-alg13-16171820-k4`](../frozen/frozen-fix-f203-2s1e-alg13-16171820-k4/) 全链路上，**不新发明算法**，仅合并两个已验收向量 fork：

| 上游探针 | 替换段 | 默认宏 |
|----------|--------|--------|
| [`pass-fix-f203-2s1e-byteencode12-vec-k4`](../pass-fix-f203-2s1e-byteencode12-vec-k4/) | 行 **19–20** `ByteEncode₁₂` | `BYTE_ENCODE12_VEC=1` |
| [`pass-fix-f203-alg11-12-multiplyntts-k4`](../pass-fix-f203-alg11-12-multiplyntts-k4/) | 行 **18** `MultiplyNTTs` basemul | `ALG11_IMPL=1` `ALG11_VEC_VARIANT=2` `ALG11_MEM_OPS=1` |

行 **16–17**（Stage1–3 NTT）保持与基线相同：平面 mat_c、`F203_STAGE1_SPLIT=1` bulk 向量 Stage1、AIC MMAD、S3 向量 merge+mod。

## 数据流（不变）

```text
host 1s+1e + â[16,256]
  → Stage1–3 NTT（行 16–17）
  → Aiv2s1eUbPipeline
       stageS3Into → ŝ/ê_hat UB
       stageHatInto → t̂（行 18）
       stageEncodeOut → ek/sk（行 19–20）
```

## 集成触点（仅 `2s1e_post_ntt_ub.hpp`）

### 行 18 — Alg.11 向量 basemul

- **替换**：`multiply_ntts_half_scalar` → `hat_alg11::multiply_ntts_half_vec`（封装 `alg11_vec::multiply_ntts_vec_dispatch`）。
- **γ 半多项式**：`gammaOff ∈ {0,64}`；每个 `subOff` 块在 `j` 循环前从 `gAlg11GammasGm + gammaOff` DataCopy 到 `gammaSlice`（不破坏 Init 灌满的 ROM γ）。
- **半多项式 interleave**：`pairCount=64` 时 ROM reorder 固定 n=256（c1@+512B），`interleave_pairs_dispatch` 回退标量交织。
- **ROM**：`Init()` 一次 `init_rom_luts_ub`（γ / Gather 字节索引 / interleave reorder），与 Alg11 toy 相同。
- **scratch**：+1792 int32（ROM 640 + VecWs 1024 + gammaSlice 128）。

### 行 19–20 — ByteEncode 向量

- 直接复用 `byteencode12-vec-k4` 的 `poly_byte_encode12_local(..., encode_ws)` + `+kVecScratchBytes`。
- 交织仍用 `pack_quad12_i32` + `DataCopy`（910B 无 Scatter）。

### 内核链接

- `mmad_custom.cpp` 在 `ALG11_MEM_OPS=1` 时 `#include "alg11_rom_tables.cpp"`（SIM 符号，与 Alg11 toy 相同）。

## 不改动

- `tiling.h` / `mixPass` / golden `gen_data.py` / `verify_result.py` — 与基线 **bit-exact** 同一套。
- NTT S1–S3：**禁止 Gather**（政策不变）；post-NTT Gather 仅用于 basemul/encode。
- 不 fork / 不参考 `frozen-fix-f203-2s1e-basemul-vec-k4`。

## 宏（CMake 默认）

| 宏 | 默认 | 含义 |
|----|------|------|
| `HAT_ALG11_VEC` | `1` | 行 18 向量 basemul；`0` 回退标量（与基线一致） |
| `BYTE_ENCODE12_VEC` | `1` | 行 19–20 向量 encode |
| `BYTE_ENCODE12_SCATTER_VEC` | `1` | pack+DataCopy 交织 |
| `ALG11_IMPL` / `ALG11_VEC_VARIANT` / `ALG11_MEM_OPS` | `1` / `2` / `1` | Alg11 向量核 |

## 验收

```bash
cd ascendc-tests/fix-f203-2s1e-alg13-16171820-vec-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```

`dst` / `t_hat` / `ek_polyvec` / `sk_polyvec` 与 golden `max_abs_diff=0`。

## 与基线关系

| 探针 | 角色 |
|------|------|
| `fix-f203-2s1e-alg13-16171820-k4` | 标量 basemul + 标量 encode；功能基线 |
| **本目录** | 向量 basemul + 向量 encode；**集成交付目标** |
| `byteencode12-vec-k4` / `alg11-12-multiplyntts-k4` | 单算子参考；验证通过后收敛到本目录 |

---

## 内积单用例对接（行 18，2026-06-18）

**前提**：alg13 与单用例探针共用 **同一 A 布局**：

```text
a_hat[(p*K+j)*N + c]   // 行主序 K×K，input/a_hat.bin
s_hat[j*N + c]
```

| 探针 | 角色 | SIM tick（参考） |
|------|------|------------------|
| [`innerproduct-k4`](../pass-fix-f203-alg11-12-innerproduct-k4/) | 4×4×1 全量单 AIV，`ProcessFullPoly` | ~43932 |
| [`innerproduct-k4-halfrows`](../pass-fix-f203-alg11-12-innerproduct-k4-halfrows/) | 4×4×1 双 AIV 半行，对齐 KeyGen 分工 | ~26230 |

**禁止** 使用已废弃的 `a_col` `(j*P_OUT+p)*N` 转置格式。

**集成步骤**：

1. `mixPass=4`：管道 ŝ + GM `a_hat`，仅跑内积（无 NTT / encode）
2. 对拍 `t_hat` 与 golden / 单用例输出
3. 将探针 `ProcessFullPoly` / `ProcessHalfRows` 迁入 `stageHatInto`；`+ê` 单独一步

纪要：[qa/2026-06/2026-06-18-内积布局与NTT内积UB融合讨论.md](../../qa/2026-06/2026-06-18-内积布局与NTT内积UB融合讨论.md) §1
