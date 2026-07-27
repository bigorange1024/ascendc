# ByteEncode₁₂ 方案（pass-fix-f203-2s1e-byteencode12-vec-k4）

**更新**：2026-06-19

## 目的

**ByteEncode₁₂-only** 探针（k=4，2×AIV）：preset ŝ‖ê + t̂ → ek/sk。输入与 [`vec-k4-v2`](../pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/) mixPass=7 **字节级一致**；实现可积木替换集成 `poly_byte_encode12_local`。

原理：[F203-ByteEncode12-prefetch技术总结.md](../../docs/notes/F203-ByteEncode12-prefetch技术总结.md)

---

## 输入契约

| 文件 | 形状 | 集成对应 |
|------|------|----------|
| `input/dst.bin` | `[12,256]` | `dst_preset` / `golden.bin` |
| `input/t_hat.bin` | `[4,256]` | `t_hat_preset` / `golden_t_hat.bin` |

`gen_data.py` 复用 v2 NTT→hat→encode golden 链。

---

## 数据流

### 默认：`BYTE_ENCODE12_PREFETCH=1`

```text
a[256] int32（UB 上连续 poly）
  │ ROM DataCopy → idxEven/idxOdd；Gather×2(128)
  ▼
t0[128], t1[128]
  │ compute_b012(128)
  ▼
b0/b1/b2[128]
  │ pack_quad12 ×32 + DataCopy 384B
  ▼
r[384] → GM ek/sk
```

### Legacy：`BYTE_ENCODE12_PREFETCH=0`（tile=32）

见 [qa/2026-06-15](../../qa/2026-06/2026-06-15-ByteEncode12向量与Scatter讨论.md) §2。

---

## 宏

| 宏 | 默认 | 含义 |
|----|------|------|
| `BYTE_ENCODE12_VEC` | `1` | 0=标量；1=向量 |
| `BYTE_ENCODE12_SCATTER_VEC` | `1` | 0=SetValue；1=pack+DataCopy |
| `BYTE_ENCODE12_PREFETCH` | `1` | 0=tile32；1=整 poly ROM+Gather×1 |

---

## 关键文件

| 文件 | 作用 |
|------|------|
| `byte_encode12_custom.cpp` | AIV-only kernel |
| `byte_encode12_only.hpp` | preset 加载 + `stageEncodeOut` |
| `byte_encode12_vec.hpp` | tile / prefetch 实现 |
| `byte_encode12_rom_tables.*` | Gather 字节索引 ROM |
| `byte_encode12_pair.hpp` | `poly_byte_encode12_local` 分发 |

---

## 性能（910B4 SIM，Alg.13 输入）

| `BYTE_ENCODE12_PREFETCH` | Total tick |
|--------------------------|------------|
| **1**（默认） | **17393** |
| 0 | 25457 |

---

## 验收

```bash
cd ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-2s1e-byteencode12-vec-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
BYTE_ENCODE12_PREFETCH=0 bash run.sh -r sim -v Ascend910B4
```

讨论：[qa/2026-06-19](../../qa/2026-06/2026-06-19-ByteEncode12-only探针与prefetch实验.md)

---

## 多 d 扩展（ByteEncode_d，Encrypt 用）

**更新**：2026-06-28 — 不另建目录；在本探针上扩展 Alg.5 参数 **d**。

### 目标 d（k=4 算法几何；本 k2 目录仅作 ML-KEM-512 W0/B2 d=12 探针）

| d | 用途 | 单 poly 字节 | 状态 |
|---|------|--------------|------|
| **12** | KeyGen `ByteEncode₁₂(t,s)` | 384 | **已 PASS**（默认路径） |
| **10** | Encrypt `ByteEncode_10(Compress_10(u))` → c₁ | 320 | 待实现 |
| **4** | Encrypt `ByteEncode_4(Compress_4(v))` → c₂ | 128 | 待实现 |
| 11 / 5 | ML-KEM-1024 备选 | 352 / 160 | 低优先级 |

### CMake / run.sh

| 宏 | 默认 | 含义 |
|----|------|------|
| `F203_BYTE_ENCODE_D` | `12` | 4 / 5 / 10 / 11 / 12 |
| `BYTE_ENCODE12_VEC` | `1` | 沿用现有向量开关 |
| `BYTE_ENCODE_D_PACK` | `auto` | d=12→`pair12`；d=4→`nibble8`；d=10→`block4x5` |

```bash
# Encrypt u 路径验收（规划）
F203_BYTE_ENCODE_D=10 bash run.sh -r sim -v Ascend910B4
F203_BYTE_ENCODE_D=4  bash run.sh -r sim -v Ascend910B4
```

### 向量策略（按 d）

- **d=12**：现有 `Gather`+ROM prefetch + `compute_b012` + `pack_quad12`（不变）。
- **d=4**：8 系数→4 字节 nibble pack；`mask_low_bits_i32(·,4)` + `ShiftRight/Or` + 顺序 `DataCopy`（无 Scatter）。
- **d=10**：4 系数→5 字节块 pack，布局对齐 mlkem-native `poly_compress_d10_c` 的 **ByteEncode 半部**（输入为已 Compress 的 10-bit 域）。
- **d=11/5**：Phase A 标量 pack；向量 ROI 低。

### 与 compress 探针衔接

[`pass-f203-compress-d-vec-k4`](../pass-f203-compress-d-vec-k4/) 输出 d-bit 域（**d=4/10**）→ 本探针 `poly_byte_encode_d_local` → GM `encoded.bin`。Phase B 可 fused 单 kernel。

### 文件演进（规划）

| 新增/改名 | 作用 |
|-----------|------|
| `byte_encode_d_config.hpp` | `F203_BYTE_ENCODE_D`、输出字节数 |
| `byte_encode_d_pack.hpp` | d 分派 pack 模板 |
| `byte_encode_d_ref.c` | Host golden（Alg.5 比特流或 mlk 布局） |
| 保留 `byte_encode12_*.hpp` | d=12 专用路径，避免回归 |

配对 decode：[`pass-f203-alg6-bytedecode-d-vec-k4`](../pass-f203-alg6-bytedecode-d-vec-k4/)。
