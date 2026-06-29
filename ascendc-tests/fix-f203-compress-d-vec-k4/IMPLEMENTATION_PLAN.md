# IMPLEMENTATION_PLAN — fix-f203-compress-d-vec-k4

**状态**：方案（未实现）  
**FIPS 203**：§4.2.1 `Compress_d`（Eq 4.7）；Alg.14 行 22–23  
**参考实现**：`thirdparty/liboqs/.../mlkem/src/compress.h`（Barrett 常数已验证）

---

## 1. 规范锚点（§4.2.1）

对 `f ∈ R_q`（系数 unsigned canonical `u ∈ [0, q-1]`，`q=3329`）：

```text
Compress_d(f) = round( (2^d / q) · f )   // 分量-wise，结果 in [0, 2^d - 1]
```

等价整数形式（mlkem-native）：

```text
Compress_d(u) = ((u · ⌊2^d · 2^N / q⌋ + 2^{N-1}) >> N) mod 2^d
```

| d | 公式 | Barrett 乘数（u32/u64） | 右移 N |
|---|------|-------------------------|--------|
| 1 | round(2u/q) | `1290168` | 31 |
| 4 | round(16u/q) mod 16 | `1290160` | 28 |
| 5 | round(32u/q) mod 32 | `1290176` | 27 |
| 10 | round(1024u/q) mod 1024 | `2642263040` (u64) | 33 |
| 11 | round(2048u/q) mod 2048 | `5284526080` (u64) | 33 |

**Encrypt 用法**（Alg.14，k=4 工程默认 ML-KEM-768 参数）：

```text
c₁ ← ByteEncode_10( Compress_10(u) )    // u ∈ R_q^k，每 poly 256 coef
c₂ ← ByteEncode_4(  Compress_4(v)  )    // v ∈ R_q
```

Table 2 备选（FIPS k=4 = ML-KEM-1024）：`d_u=11`, `d_v=5`。

---

## 2. 目录骨架

```text
fix-f203-compress-d-vec-k4/
├── IMPLEMENTATION_PLAN.md
├── STATUS.md
├── compress_d_config.hpp           # F203_COMPRESS_D = 4|5|10|11
├── compress_d_ref.c/h
├── compress_d_vec.hpp              # 核心向量核
├── compress_d_barrett.hpp          # 常数 + 宽乘模板
├── compress_d_custom.cpp           # AIV-only：in[256] → out[256] d-bit
├── run.sh / scripts/
```

**与 ByteEncode 关系**：本探针输出 **256×int32 压缩域**（值 < 2^d）；[`fix-f203-byteencode-d-vec-k4`](../fix-f203-byteencode-d-vec-k4/) 负责后续 `ByteEncode_d` 打包（d=4/10）。d=12 KeyGen 仍用 [`pass-fix-f203-2s1e-byteencode12-vec-k4`](../pass-fix-f203-2s1e-byteencode12-vec-k4/)。

---

## 3. AscendC 向量 API 映射

### 3.1 统一向量模式（d=4,5 — u32 Barrett）

输入：`a[256]` int32 UB，`a[i] ∈ [0,3328]`。

```cpp
// 伪代码 — 128-wide tile
Muls(d0, aTile, static_cast<int32_t>(BARRETT_D4), 128);  // u32 overflow OK
Adds(d0, d0, static_cast<int32_t>(1 << 27), 128);
ShiftRight(out, d0, 28, 128);
// mod 16: mask_low_bits_i32(out, tmp, 4, 128);
```

**API**：`Muls`, `Adds`, `ShiftRight`, `Sub`（mask 模板复用 byteencode12 `mask_low_bits_i32`）。

**并行度**：256 coef → **2×128** 或 **4×64** tile；单 AIV 即可（无跨 poly 依赖）。

### 3.2 d=10 / d=11 — u64 路径

`u · 2642263040` 超 u32；策略：

**方案 P1（推荐）**：`int64` LocalTensor 或拆分为 **两次 u32 Barrett**（mlk 用 u64；设备可用 `Mul` 宽指令若可用，否则 **Hi/Lo 分解**）。

**方案 P2**：预计算 `floor(2^10 * u / q)` 查找表 — **否决**（ROM 过大）。

**方案 P3（Phase A）**：d=10 用 **标量** Barrett 对照向量 d=4；SIM 验证后再上 u64 向量。

AscendC 实现 u64 tile：

```text
lo = (uint32)u * (uint32)C
hi = MulHigh(u, C)   // 若 API 无 MulHigh：用 16-bit 分解或标量 fallback
t  = (hi << k | lo >> ...) + round_bias
out = t & ((1<<d)-1)
```

查阅 `AscendC::Mul` / 向量乘加文档；若无 64-bit 向量乘，**d=10 热路径**可 64 系数/批标量 + 其余向量 OR 全 poly 256 标量（仍快于 Encrypt 其余部分）。

### 3.3 d=1（Compress_1 / tomsg）

Alg.14 Decrypt 侧 `ByteEncode_1(Compress_1(w))` 亦需；向量：

```cpp
Muls(d0, u, 1290168, 128);
Adds(d0, d0, 1<<30, 128);
ShiftRight(bit, d0, 31, 128);  // 0/1
```

8 系数聚合成 1 字节 — **第二阶段** bit-pack（可放 byteencode12 探针 d=1 扩展或本探针 extension）。

---

## 4. 探针 I/O

| 文件 | 形状 | 说明 |
|------|------|------|
| `input/poly.bin` | `[256]` int32 | canonical mod q |
| `input/polyvec.bin` | `[4,256]` | Encrypt `u` 模拟 |
| `output/comp.bin` | `[256]` int32 | 值 ∈ [0,2^d-1] |

Golden：`compress_d_ref.c` 逐系数 mlk 同构；`verify` max diff = 0。

**可选第二输出**：直接接 byteencode12 探针（多 d）得 `encoded.bin` 做 Encrypt `c₁` 端到端。

---

## 5. 性能预期

| d | 向量难度 | 预期 SIM（256 coef，单 AIV） |
|---|----------|-------------------------------|
| 4 | 低（纯 u32） | <3k tick（估） |
| 10 | 中（u64） | 待测；仍 ≪ NTT |
| 11 | 中 | 同 d=10 |

Encrypt 瓶颈仍在 NTT/内积；Compress 必须 **正确 + 可融合**，非首要 tick 优化。

---

## 6. 实现阶段

| 阶段 | 内容 |
|------|------|
| A0 | ref + 标量 device，d=4,10 |
| A1 | 向量 d=4 + d=5 |
| A2 | 向量 d=10（u64 方案定稿） |
| A3 | k=4 polyvec batch（4×256 一次 launch 或 2×AIV 分片） |
| B | fused `ByteEncode_d(Compress_d(·))` with byteencode12 探针 |

---

## 7. 集成 Encrypt

```text
// mmad_custom / encrypt compute 段（规划）
poly_compress_d10_local(comp_u, u_intt);           // 本探针
poly_byte_encode_d10_local(c1_slice, comp_u);      // byteencode12 探针（d=10）
poly_compress_d4_local(comp_v, v);
poly_byte_encode_d4_local(c2, comp_v);
```

**输入契约**：`u,v` 为 INTT 后 **canonical** `[0,q-1]`（与 KeyGen `t_hat` 同源规约）。

---

## 8. 风险

- **u32 溢出**：Barrett 乘法 **必须** 允许 wrap（mlk CBMC 注释）；勿转 int64 再 clamp 改变语义。
- **d=10 u64**：910B4 向量 64-bit 乘支持需查 CANN 版本；准备标量 fallback 宏。
- **与 Decompress 不对逆**：Compress 探针只验 forward；逆运算见 decompress 探针 + round-trip 噪声测试（Encrypt/Decrypt 环）。
