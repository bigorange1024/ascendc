# IMPLEMENTATION_PLAN — pass-f203-alg6-bytedecode-d4-d10-vec-k4

**状态**：**pass**（A0：d=**4** / d=**10** CPU+SIM PASS）  
**验收 d**：**`{4, 10}`** only  
**FIPS 203**：Algorithm 6 `ByteDecode_d`；§4.2 + §2.4.8  
**配对探针**：[`pass-f203-byteencode-d4-d10-vec-k4`](../pass-f203-byteencode-d4-d10-vec-k4/)（Encrypt **d=4/10**）；d=12 见 [`pass-fix-f203-2s1e-byteencode12-vec-k4`](../pass-fix-f203-2s1e-byteencode12-vec-k4/)

---

## 1. 规范锚点

### 1.1 Algorithm 6

```text
F ← zeroes(256)
i ← 0
for j = 0 .. 255:
    x ← ( B[⌊i/8⌋] mod 2^d )   // 取从 bit i 开始的 d 个低位
    F[j] ← x
    i ← i + d
```

**d=12** 等价于现有 `poly_frombytes` / `ByteDecode_12`（2 系数 / 3 字节）。  
**Decrypt 路径**（Alg.15）：`u' ← Decompress_du( ByteDecode_du(c₁) )` — 本探针只做 **ByteDecode**（**d=4/10**）；Decompress 见 [`pass-f203-decompress-d4-d10-vec-k4`](../pass-f203-decompress-d4-d10-vec-k4/)。

### 1.2 Encrypt 相关 d（k=4 polyvec）

| d | 输入字节/poly | Decrypt 用途 |
|---|---------------|--------------|
| 10 | 320 | `ByteDecode_10(c₁)` → 10-bit 域 |
| 4 | 128 | `ByteDecode_4(c₂)` → 4-bit 域 |
| 12 | 384 | KeyGen 侧 sk/t 解码（非 Encrypt 热路径） |

---

## 2. 目录骨架

```text
pass-f203-alg6-bytedecode-d4-d10-vec-k4/
├── IMPLEMENTATION_PLAN.md
├── STATUS.md
├── byte_decode_d_config.hpp
├── byte_decode_d_ref.c/h
├── byte_decode_d_vec.hpp
├── byte_decode_d_unpack.hpp      # d=4/10/12 逆 pack
├── byte_decode_d_custom.cpp
├── run.sh / CMakeLists.txt / scripts/
```

---

## 3. AscendC 向量策略

Decode 是 Encode 的 **unpack + 位域提取**，向量难点在 **跨字节边界**。按 d 分派：

### 3.1 d=12 — 复用 Kyber frombytes 向量

参考 mlk `poly_frombytes_c` / byteencode12 逆：

```text
t0 = b[3i+0] | ((b[3i+1] & 0x0F) << 8)
t1 = (b[3i+1] >> 4) | (b[3i+2] << 4)
```

向量（128 pair / tile）：

1. `DataCopy` 384B poly → UB `uint8`
2. `Gather` 或固定 offset 加载 `b0,b1,b2` 三向量（ROM 索引 `3*i`, `3*i+1`, `3*i+2`）
3. `ShiftRight` / `ShiftLeft` / `Or` / `And` 得 `t0,t1` → 写 `int32[256]`
4. **无 mod q**（ByteDecode 输出 `[0,2^12-1]`；后续 Decompress 或 KeyGen 再规约）

**API**：`Gather`, `ShiftRight`, `ShiftLeft`, `And`, `Or`, `Duplicate`, `DataCopy`.

### 3.2 d=4 — nibble 解包

8 系数 ← 4 字节；逆 d=4 encode：

1. 加载 128B tile → `b[128]`
2. `lo = b & 0x0F`，`hi = (b >> 4) & 0x0F`（`ShiftRight`+`And` on widened int32）
3. 交织写回 `F[2i]`, `F[2i+1]`（`Duplicate` / 交错 store 或双 `DataCopy` 到 scratch 再 merge）

**宽度**：一次 128 字节 → 256 个 4-bit 值（正好 1 poly）。

### 3.3 d=10 — 5 字节 → 4 系数

逆 mlk `poly_decompress_d10_c` 的 **bit 重组**（先于 Decompress）：

```text
t[0] = b[0] | (b[1]<<8)
t[1] = (b[1]>>2) | (b[2]<<6)
t[2] = (b[2]>>4) | (b[3]<<4)
t[3] = (b[3]>>6) | (b[4]<<2)
// 各 & 0x3FF
```

向量：

1. 每 5B 一组，64 组/tile（320B/poly 一次 UB）
2. 用 **常量 shift/mask 向量**（`Duplicate` 广播 mask `0x3FF`）提取 `t0..t3`
3. `Scatter` **不可用** → 将 4×64 系数 **顺序写入** `out[256]`（4 段 `DataCopy` 或手动 lane 合并）

**实现提示**：预生成 ROM `group_base[64] = {0,5,10,...}`，`Gather` 从 `b` 取 5 字节窗口（5 次 Gather 或 uint8→int32 宽 load + shift 组合）。

### 3.4 d=11 / d=5

同 encode 计划：向量 unpack 复杂度高；Phase A 标量 unpack + golden；Phase B 仅对 d=4/10/12 向量化。

---

## 4. 探针 I/O

| 文件 | 形状 |
|------|------|
| `input/encoded.bin` | `poly_bytes(d)` 或 k=4 polyvec |
| `output/poly.bin` | `[256]` int32（d-bit 值，未 Decompress） |

**Golden 链**：`encoded = ByteEncode_d(F)` → device decode → cmp `F`（round-trip）；另备独立 random `encoded` 用 ref decode 对拍。

**Round-trip 测试**：与 [`pass-fix-f203-2s1e-byteencode12-vec-k4`](../pass-fix-f203-2s1e-byteencode12-vec-k4/) 共享 vectors，确保 `Decode(Encode(x))=x mod 2^d`。

---

## 5. 与 Decompress 的接口

```text
// 设备 API（规划）
void poly_bytedecode_d_local(LocalTensor<int32_t>& out, LocalTensor<uint8_t>& in, int d);
void poly_decompress_du_fused(...);  // 后续：Decode 输出直接进 Decompress 向量槽
```

Decrypt 全链：`c₁` GM → **ByteDecode_10** → **Decompress_10** → `u'[256]`（×k）。

---

## 6. 实现阶段

| 阶段 | 内容 |
|------|------|
| A0 | Host ref + round-trip with byteencode12 探针 golden |
| A1 | d=12 / d=4 向量 decode |
| A2 | d=10 向量 decode（320B in） |
| B | Fused `Decompress_d(ByteDecode_d(·))` 见 decompress 探针 |

---

## 7. 风险

- **位边界**：d=10/11 unpack 错误会静默破坏 Decrypt；必须 round-trip + liboqs 向量对照。
- **UB 布局**：uint8 与 int32 视图切换需 `ReinterpretCast`，对齐 32B。
- **Gather 索引**：仅 byte 索引 ROM，禁止 NTT limb Gather 模式。
