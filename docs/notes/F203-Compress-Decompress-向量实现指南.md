# FIPS 203 Compress_d / Decompress_d — AscendC 向量实现指南

**读者**：后续 Encrypt/Decrypt pack、tail 探针抄码者  
**探针**：[`pass-f203-compress-d-vec-k4`](../../ascendc-tests/pass-f203-compress-d-vec-k4/) · [`pass-f203-decompress-d-vec-k4`](../../ascendc-tests/pass-f203-decompress-d-vec-k4/)  
**定点契约**：[`F203-PKE-liboqs交叉验证与Compress定点技术总结.md`](F203-PKE-liboqs交叉验证与Compress定点技术总结.md)

---

## 1. 数学（不变量）

```text
Compress_d(u)   = round(u · 2^d / q) mod 2^d     // u ∈ [0,q)
Decompress_d(c) = round(c · q / 2^d)            // c ∈ [0,2^d)
```

`q=3329`，单 poly `N=256`。CMake 选档：`F203_COMPRESS_D` / `F203_DECOMPRESS_D` ∈ **{4,5,10,11}**。

| 参数集 | u / c₁ 常用 d | v / c₂ 常用 d |
|--------|---------------|---------------|
| ML-KEM-768（k=3 档类推） | 10 | 4 |
| ML-KEM-1024（k=4） | 11 | 5 |

---

## 2. 向量路径选型（勿与 Stage3 mod q 混用）

本仓 **NTT Stage3** 有两套 **`x mod q`** 向量约化（见 `stage3_mod_variants.hpp`）：

| 路线 | 用途 | 能否直接用于 Compress？ |
|------|------|-------------------------|
| Barrett `μ·x>>k` | 大整数 **mod q 余数** | ❌ Compress 要的是 **商** `round(u·2^d/q)` |
| cast_div `Cast→Div→商→×q→Sub` | **mod q 余数** | ❌ 同上；但 **商路径可改造** |

**Compress 专用两条路**（探针内已验收）：

### P-COMP-1：int32 Barrett（d=4、d=5）

`u·M_d` 可放进 u32 lane，全程 `Muls`/`Adds`/`ShiftRight`/`mask_low_bits`：

| d | M_d | bias | shift |
|---|-----|------|-------|
| 4 | 1290160 | `1<<27` | 28 |
| 5 | 1290176 | **`1<<26`**（勿用 `1<<27`） | 27 |

### P-COMP-2：cast_div 商（d=10、d=11）

不求余数，只求 **floor((u·2^d + q/2)/q)**：

```text
Muls(u, 2^d) → Adds(q/2) → Cast float → Div/3329 → CAST_TRUNC → mask d bits
```

操作数 < 2²³，float32 与 liboqs Barrett 标量 **全 u∈[0,q) 0 差异**（2026-07-08 脚本验证）。

SIM 参考：d=10 标量 ~10713 tick → cast_div 向量 **~3449**。

### P-DEC：int32 线性（d=4/5/10/11 统一）

Decompress 无 u64：`Muls(c,q) → Adds(2^{d-1}) → ShiftRight(d)`。bias 表：

| d | bias | shift |
|---|------|-------|
| 4 | 8 | 4 |
| 5 | 16 | 5 |
| 10 | 512 | 10 |
| 11 | 1024 | 11 |

---

## 3. 抄码清单

| 文件 | 内容 |
|------|------|
| `f203_compress_d_params.hpp` | 编译期 d 常数、Barrett/cast_div 开关 |
| `compress_d_vec.hpp` | `poly_compress_barrett_vec` / `poly_compress_cast_div_vec` |
| `f203_decompress_d_params.hpp` | bias/shift |
| `decompress_d_vec.hpp` | `poly_decompress_local` |

**float UB**：d=10/11 Compress 需 `3×256×sizeof(float)` VECCALC（与 Stage3 cast_div 同量级）。

---

## 4. 验收

```bash
# Compress：逐 d
for d in 4 5 10 11; do F203_COMPRESS_D=$d bash run.sh -r cpu -v Ascend910B4; done

# Decompress：逐 d
for d in 4 5 10 11; do F203_DECOMPRESS_D=$d bash run.sh -r cpu -v Ascend910B4; done
```

对拍：`max diff = 0` vs `compress_d_ref.c` / `decompress_d_ref.c`。

---

## 5. 与 tail pack / ByteEncode 的关系

[`pass-fix-f203-alg14-lines2-24-encrypt-compute-tail-k4`](../../ascendc-tests/pass-fix-f203-alg14-lines2-24-encrypt-compute-tail-k4/)（及前身 tail-only 探针）已 vendored **d=5 Barrett + d=11 cast_div** 至 `compute/f203_tail_compress_byteencode.hpp`（**Compress 向量、`COMPRESS_D_VEC=1` 路径**；禁止跨探针 `#include`）。

**ByteEncode / ByteDecode 选型**（bit 重组 vs per-lane）见定稿：[`F203-ByteEncode-ByteDecode-d-向量与标量选型.md`](F203-ByteEncode-ByteDecode-d-向量与标量选型.md)。

- **Encrypt tail**：ByteEncode **标量逐组 pack**（`BYTE_ENCODE_D_VEC=1`）；**不激活** `VEC=2` 真·Gather pack（实验更慢 +7%/+12% tick）。
- **Decrypt**：ByteDecode d=5/11 **标量逐组 unpack**；Decompress **向量**（本指南 §2 P-DEC）。

**Decompress 不在 encrypt tail 范围**；Decrypt 侧用 `pass-f203-decompress-d-vec-k4` 同理抄码。
