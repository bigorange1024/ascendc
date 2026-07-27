# STATUS — encode/（pass-fix-f203-byteencode-decode-d-k2）

**曾用目录名**：`pass-f203-byteencode-d4-d10-vec-k4`（2026-07-08 更名为 `*-d-vec-k4`，与 compress/decompress 探针对齐）。

**512 W0/B2 口径**：顶层默认只验收 ML-KEM-512 密文域 **d=4/10**；实现仍保留通用 d=5/11 代码作为活跃 k3/k4 探针的同源算法路径。

**验收 d 集合**：**`d ∈ {4, 5, 10, 11}`**（FIPS 203 Alg.5 **`ByteEncode_d`**；输入为已压缩的 d-bit 系数）。

| d | 典型用途 | out bytes |
|---|----------|-----------|
| 4 | ML-KEM-512/768 c₂ | 128 |
| 5 | ML-KEM-1024 c₂ | 160 |
| 10 | ML-KEM-512/768 c₁ | 320 |
| 11 | ML-KEM-1024 c₁ | 352 |

## 实现要点

- **d=4/10**：8 系数/4 系数分组 + 向量 `mask_low_bits` + 标量 pack（与既有路线一致）。
- **d=5/11**：**8 系数/组** + 向量 mask + 分组标量 pack（公式对齐 ml-kem-native `poly_compress_d5/d11` 比特布局，与 Alg.5 比特流 0-diff）。
- **非**纯向量比特流：pack 环仍为标量 `SetValue`，循环 O(N/8) 逐组。
- **宏分层**：`BYTE_ENCODE_D_VEC=0` 纯标量；`=1` **默认验收**（mask+标量 pack）；`=2` 真·向量 pack（**保留代码、默认不激活**，见 §VEC=2）。

## 性能（910B4，BYTE_ENCODE_D_VEC=1）

| d | out bytes | CPU | SIM totalTick |
|---|-----------|-----|---------------|
| 4 | 128 | PASS | **5435** |
| 5 | 160 | PASS | **5537** |
| 10 | 320 | PASS | **6455** |
| 11 | 352 | PASS | **6568** |

## VEC=2 真·向量 pack 实验（2026-07-08，仅 d=5/d=11）

**动机**：Encrypt tail（`pass-fix-f203-alg14-lines2-24-encrypt-compute-tail-k4`）里 ByteEncode 的 bit-pack 是标量逐组，
拟仿 `byte_encode12_vec.hpp`（d=12：2 系数=3 字节，字节对齐）把 d=5/d=11 也做成真·向量：
**Gather 每组 8 个 position-lane → 向量算 byte-lane（Muls/ShiftRight/Add）→ 每 4 组拼整字 → 批量 DataCopy**。
实现见 `byte_encode_d_vec.hpp` 的 `poly_byte_encode_d5/d11_vecpack`（`BYTE_ENCODE_D_VEC=2`）。

**结论：正确但更慢，不采纳（tail 保持 VEC=1 标量逐组 pack）。**

| d | 正确性(CPU/SIM) | VEC=1 SIM tick（标量逐组） | VEC=2 SIM tick（真·向量，trim 后） |
|---|---|---|---|
| 5 | 0-diff / 0-diff | **5464** | 5839（+7%） |
| 11 | 0-diff / 0-diff | **6604** | 7404（+12%） |

**原因**：d=5/d=11 每系数 5/11 bit **不按 2 系数=整字节** 对齐，拼字仍需逐 lane 标量 `GetValue`（无法像 d=12 靠 3 字节整字省下）；
真·向量版**净增** 8 次 `Gather` + 8 组向量算术，收益被 Gather 开销吃掉。去掉防御性掩码、合并 barrier 后（trim）差距缩小但仍慢。
→ **d=12 能向量化是因 2×12=24bit=3B 字节对齐；d=5/d=11 无此性质，标量逐组 pack 才是更优基线。**

## 环境变量

| 变量 | 默认 | 含义 |
|------|------|------|
| `F203_BYTE_ENCODE_D` | `4` | **4 / 5 / 10 / 11** |
| `BYTE_ENCODE_D_VEC` | `1` | 0 标量 / 1 向量 mask+标量拼字（默认，验收基线）/ 2 真·向量 pack（仅 d=5/d=11，实验：更慢，不采纳） |

**定稿笔记**：[`docs/notes/F203-ByteEncode-ByteDecode-d-向量与标量选型.md`](../../docs/notes/F203-ByteEncode-ByteDecode-d-向量与标量选型.md)

## 衔接

- round-trip：[`pass-f203-alg6-bytedecode-d-vec-k4`](../pass-f203-alg6-bytedecode-d-vec-k4/)
- 上游 Compress：[`pass-f203-compress-d-vec-k4`](../pass-f203-compress-d-vec-k4/)
