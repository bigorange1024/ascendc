# STATUS — pass-f203-alg6-bytedecode-d-vec-k4

**曾用目录名**：`pass-f203-alg6-bytedecode-d4-d10-vec-k4`（2026-07-08 更名；验收 d 扩至 4/5/10/11）。

**前缀 `pass-`**：本目录 **d=4/5/10/11** 单用例 CPU+SIM 均已验收。

**验收 d 集合**：**`d ∈ {4, 5, 10, 11}`**（FIPS Alg.6 **`ByteDecode_d`**；输出 d-bit 整数，**不含** Decompress）。

## 目标

Decrypt 侧密文解包半部（单 poly）。

**定稿笔记**：[`docs/notes/F203-ByteEncode-ByteDecode-d-向量与标量选型.md`](../../docs/notes/F203-ByteEncode-ByteDecode-d-向量与标量选型.md)

## 配对探针

- [`pass-f203-byteencode-d-vec-k4`](../pass-f203-byteencode-d-vec-k4/) round-trip golden
- 下游 Decompress：[`pass-f203-decompress-d-vec-k4`](../pass-f203-decompress-d-vec-k4/)
- **d=12** KeyGen：[`pass-fix-f203-2s1e-byteencode12-vec-k4`](../pass-fix-f203-2s1e-byteencode12-vec-k4/)

## 实现要点与宏分层

| d | `BYTE_DECODE_D_VEC=0` | `BYTE_DECODE_D_VEC=1`（默认） | 真·向量 unpack（VEC=2） |
|---|------------------------|-------------------------------|-------------------------|
| 4 | 标量 `unpack_d4_pair` | 向量 nibble mask + 标量 scatter 到 out | **未实现** |
| 5 / 10 / 11 | 标量 `unpack_*_group` 逐组 | **与 VEC=0 同体**（仅 d=4 的 VEC 分支有差异） | **未做** encode 对称实验；预期与 encode VEC=2 类似（Gather 开销 > 标量逐组） |

- **d=5/11**：bit 重组与 encode pack **对称**，均为标量逐组 `GetValue/SetValue`（O(N/8)）；`VEC=1` 命名沿用 encode 探针，**不表示** d=5/11 有额外向量 unpack。
- **d=12** 真·向量 decode 见 [`pass-fix-f203-2s1e-byteencode12-vec-k4`](../pass-fix-f203-2s1e-byteencode12-vec-k4/) 配对路径（2 系数=3B 对齐，与 encode12 同构）。

## 性能（910B4，BYTE_DECODE_D_VEC=1）

| d | in bytes | CPU | SIM totalTick |
|---|----------|-----|---------------|
| 4 | 128 | PASS | **9186** |
| 5 | 160 | PASS | **5696** |
| 10 | 320 | PASS | **6546** |
| 11 | 352 | PASS | **6641** |

## 环境变量

| 变量 | 默认 | 含义 |
|------|------|------|
| `F203_BYTE_DECODE_D` | `4` | **4 / 5 / 10 / 11** |
| `BYTE_DECODE_D_VEC` | `1` | 0=标量 / 1=**默认**（d=4 向量 nibble；d=5/10/11 标量逐组 unpack，与 0 同路径） |
