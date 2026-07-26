# IMPLEMENTATION_PLAN — pass-f203-decompress-d-vec-k4

**状态**：**pass**（d=**4/5/10/11** CPU+SIM）  
**验收 d**：**`{4, 5, 10, 11}`**  
**FIPS 203**：§4.2.1 `Decompress_d`（Eq 4.8）；Alg.15 行 4  
**配对**：[`pass-f203-compress-d-vec-k4`](../pass-f203-compress-d-vec-k4/)（近似逆，非精确）

---

## 1. 规范锚点（§4.2.1）

对 compressed 值 `u ∈ [0, 2^d - 1]`：

```text
Decompress_d(u) = round( (q / 2^d) · u )   // 输出 ∈ [0, q-1] canonical
```

mlkem-native 整数形式：

| d | 实现 |
|---|------|
| 4 | `((u * q) + 8) >> 4` |
| 5 | `((u * q) + 16) >> 5` |
| 10 | `((u * q) + 512) >> 10` |
| 11 | `((u * q) + 1024) >> 11` |

**Decrypt 链**（Alg.15）：

```text
u' ← Decompress_du( ByteDecode_du(c₁) )
v  ← Decompress_dv( ByteDecode_dv(c₂) )
```

本探针输入 **d-bit 域** `int32[256]`（ByteDecode 输出或 golden 直接生成）；输出 **canonical mod q**。

---

## 2. 目录骨架

```text
pass-f203-decompress-d-vec-k4/
├── IMPLEMENTATION_PLAN.md
├── STATUS.md
├── decompress_d_config.hpp
├── decompress_d_ref.c/h
├── decompress_d_vec.hpp
├── decompress_d_custom.cpp
├── run.sh / scripts/
```

---

## 3. AscendC 向量 API 映射

Decompress 是 **纯 per-lane 线性运算**，最适合向量化（无 bit shuffle）。

### 3.1 统一模板（所有 d）

常量：`Q = 3329`，`bias = 2^{d-1}`（即 8/16/512/1024）。

```cpp
// 128-wide tile, in/out int32
Muls(prod, inTile, static_cast<int32_t>(MLKEM_Q), 128);  // u * q
Adds(prod, prod, static_cast<int32_t>(BIAS_D), 128);
ShiftRight(out, prod, SHIFT_D, 128);  // SHIFT_D = d
// 输出已 in [0,q-1]，一般无需再 mod q（Decompress 定义保证）
```

**API**：`Muls`, `Adds`, `ShiftRight` — 与 NTT 后 mod 规约同类，**无 Gather**。

### 3.2 宽乘注意

`u * 3329`：`u < 2^11` → product < 2^23，**u32 安全**（d=11 最坏 2047×3329 ≈ 6.8M）。

### 3.3 k=4 polyvec

256×4 = 1024 lanes；单 AIV 4 tile×256 或 2×AIV 各 2 poly（对齐 Encrypt 分片习惯）。

### 3.4 Fused：`Decompress_d(ByteDecode_d(B))`

Phase B 与 [`pass-f203-alg6-bytedecode-d-vec-k4`](../pass-f203-alg6-bytedecode-d-vec-k4/) 合并：

```text
GM c₁[320] → UB uint8 → unpack d=10 → int32[256] d-bit → Decompress → u'[256]
```

减少 UB 往返；unpack 段见 alg6 计划，decompress 段用本节 3.1。

---

## 4. 探针 I/O

| 文件 | 形状 |
|------|------|
| `input/comp.bin` | `[256]` int32，值 ∈ [0,2^d-1] |
| `output/poly.bin` | `[256]` int32 canonical |

Golden：`decompress_d_ref.c`（mlk `scalar_decompress_d*` 同构）。

**噪声测试**（Encrypt 环）：

```text
x canonical → Compress_d → Decompress_d → x'
|x' - x| ≤ compress noise bound（Host Python 统计）
```

与 compress 探针联合脚本 `scripts/roundtrip_noise.py`。

---

## 5. Encrypt / Decrypt 参数（k=4）

| 参数 | d_u | d_v | c₁ 字节 | c₂ 字节 |
|------|-----|-----|---------|---------|
| ML-KEM-768 主路径 | 10 | 4 | 1280 | 128 |
| ML-KEM-1024 备选 | 11 | 5 | 1408 | 160 |

CMake：`F203_DECOMPRESS_D` 与 alg6/compress、byteencode12 探针同名参数同步。

---

## 6. 实现阶段

| 阶段 | 内容 |
|------|------|
| A0 | ref + 标量，d=4,10 |
| A1 | 向量 128-wide，d=4,5,10,11 全 d 一次模板 |
| A2 | polyvec k=4 batch |
| B | fused decode+decompress（alg6+本探针） |
| C | Decrypt 子链 SIM（接 NTT 内积后） |

---

## 7. 性能预期

Decompress 极轻（3 条向量指令/lane）；SIM tick 应 **<2k** / poly。Decrypt 热点仍在 NTT 与内积。

---

## 8. 风险

- **与 Compress 近似逆**：round-trip 测试必做，不能仅 golden 单向对拍。
- **ByteDecode 边界错误**会放大为 Decompress 输入错误 — fused 前须 alg6 独立 PASS。
- **signed 视图**：全程 unsigned canonical int32，勿用 int16 窄化（d=10 中间值 >2048）。
