# FIPS 203 ByteEncode_d / ByteDecode_d — 向量与标量选型（d=4/5/10/11）

**读者**：Encrypt tail pack、Decrypt 解包抄码者；审「是否该上真·向量 bit 流」时先读本文。  
**配对探针**：[`pass-f203-byteencode-d-vec-k4`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-f203-byteencode-d-vec-k4/) · [`pass-f203-alg6-bytedecode-d-vec-k4`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-f203-alg6-bytedecode-d-vec-k4/)  
**Compress/Decompress**（per-lane，默认向量）：[`F203-Compress-Decompress-向量实现指南.md`](F203-Compress-Decompress-向量实现指南.md)  
**d=12 真·向量 bit 流**（2 系数=24bit=3B）：[`F203-ByteEncode12-prefetch技术总结.md`](F203-ByteEncode12-prefetch技术总结.md)

---

## 1. 原理：两类算子，两种默认

FIPS 203 密文侧四算子可分成 **per-lane 算术** 与 **跨字节 bit 重组** 两类；AscendC 向量友好度完全不同。

| 类 | 算子 | 每 lane 独立？ | bit shuffle？ | 默认宏 | 默认激活向量？ |
|----|------|----------------|---------------|--------|----------------|
| **A** | Compress_d、Decompress_d | ✓ | ✗ | `*_D_VEC=1` | **是**（验收基线） |
| **B** | ByteEncode_d、ByteDecode_d | 部分 | **是**（d=5/11 尤甚） | encode `=1`；decode `=1`（d=5/11 与 0 同体） | **否**（bit 流用标量逐组；encode 的 `VEC=2` **保留不激活**） |

**不变量**：

- **类 A**：输出只依赖单个系数 `u[c]` 或 `comp[c]`，无跨系数依赖 → `Muls/Adds/ShiftRight/Cast/Div` 全 poly 一次完成。
- **类 B**：Alg.5/6 把 `d` bit 系数 **打包进字节流**；d=5（8×5=40bit→5B）、d=11（8×11=88bit→11B）**组内 bit 跨字节边界**，无法像 d=12（2×12=24bit=**整 3 字节**）那样「向量算 byte-lane + 整字 DataCopy」而无标量拼字。

**可复用模式 P-BIT-1（标量逐组）**：8 系数（或 4 系数）为一组，`pack_*_group` / `unpack_*_group` 用 `GetValue/SetValue`，循环 **O(N/8)**，与 ml-kem-native 比特布局 0-diff。

**可复用模式 P-BIT-2（d=12 真·向量）**：`Gather` 偶/奇去交错 → 向量算 b0/b1/b2 → 4 组拼 int32 字 → `DataCopy`（拼字仍少量标量 GetValue）。**仅当系数 bit 与字节边界对齐时划算**。

---

## 2. 宏开关分层（强制约定）

代码**保留**向量实验路径，用宏切换；**默认只激活已验收、SIM 更优或持平的路径**。

### 2.1 Compress / Decompress

| 探针 | 宏 | 0 | 1（默认） |
|------|-----|---|-----------|
| compress | `COMPRESS_D_VEC` | 标量 per-coeff | **向量** Barrett（d=4/5）或 cast_div 商（d=10/11） |
| decompress | `DECOMPRESS_D_VEC` | 标量 per-coeff | **向量** `Muls(q)+Adds(2^{d-1})+ShiftRight(d)` |

Encrypt tail **抄 Compress 向量路径**（`f203_tail_compress_byteencode.hpp`）。Decrypt **抄 Decompress 向量路径**。

### 2.2 ByteEncode

| 值 | 含义 | 默认？ | d=5/d=11 |
|----|------|--------|----------|
| `BYTE_ENCODE_D_VEC=0` | 纯标量 pack | 否 | 标量逐组 |
| `BYTE_ENCODE_D_VEC=1` | 向量 `mask_low_bits` + **标量逐组 pack** | **是（验收基线）** | 标量逐组（mask 可由上游 Compress 省略） |
| `BYTE_ENCODE_D_VEC=2` | 真·向量 pack（Gather+向量 byte-lane+整字 DataCopy） | **否（保留代码、不激活）** | 见 §3 |

### 2.3 ByteDecode

| 值 | 含义 | 默认？ | d=5/d/10/11 |
|----|------|--------|-------------|
| `BYTE_DECODE_D_VEC=0` | 标量 unpack | 否 | 标量逐组 |
| `BYTE_DECODE_D_VEC=1` | d=4：向量 nibble mask + 标量 scatter；d=5/10/11：**与 0 同体** | **是** | **仅标量逐组** |
| `BYTE_DECODE_D_VEC=2` | — | **未实现** | 对称 encode VEC=2；预期 Gather 开销 > 收益，未投入 |

---

## 3. VEC=2 实验结论（2026-07-08，仅 ByteEncode d=5/d=11）

**动机**：审 [`pass-fix-f203-alg14-lines2-24-encrypt-compute-tail-k4`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg14-lines2-24-encrypt-compute-tail-k4/) 时问「ByteEncode 是否该抄真·向量写法」。

**方法**：在 `pass-f203-byteencode-d-vec-k4` 实现 `poly_byte_encode_d5/d11_vecpack`（仿 `byte_encode12_vec.hpp`：`Gather` 8 position-lane → 向量 byte-lane → 4 组拼字 → 批量 `DataCopy`）。

**结果**（910B4 SIM，trim 后仍慢）：

| d | CPU/SIM 正确性 | VEC=1 tick | VEC=2 tick | Δ |
|---|----------------|------------|------------|---|
| 5 | 0-diff | **5464** | 5839 | +7% |
| 11 | 0-diff | **6604** | 7404 | +12% |

**判决**：**正确但不采纳**；tail 与默认验收均用 **VEC=1 标量逐组 pack**；VEC=2 **留在探针内供对照，编译默认不走到该分支**。

**原因**：拼字阶段仍需逐 lane 标量 `GetValue`；净增 8×`Gather` + 向量算术，无 d=12 式 3B 整字搬出收益。

---

## 4. Encrypt / Decrypt 链路上的抄码约定

```text
Encrypt tail (Alg.14 行 22–24):
  u/v ──► [Compress  向量 COMPRESS_D_VEC=1] ──► [ByteEncode 标量 pack VEC=1] ──► c

SIM Phase C（单 launch）：上述 pack 内联至 f203_encrypt_l18_l19 尾部，双 AIV 分片写 c。

Decrypt (Alg.15 等):
  c ──► [ByteDecode 标量 unpack d=5/11] ──► [Decompress 向量 DECOMPRESS_D_VEC=1] ──► u'/v'
```

**集成探针**：[`pass-fix-f203-alg14-lines2-24-encrypt-compute-tail-k4`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg14-lines2-24-encrypt-compute-tail-k4/) — SIM **1 launch**（`f203_tail_pack_ops.hpp` 内联）；CPU 仍独立 `f203_encrypt_alg14_pack`。

---

## 5. 验收命令（摘录）

```bash
# Encode：默认 VEC=1
for d in 5 11; do F203_BYTE_ENCODE_D=$d bash run.sh -r cpu -v Ascend910B4; done

# Encode VEC=2 对照（不采纳，仅实验）
F203_BYTE_ENCODE_D=5 BYTE_ENCODE_D_VEC=2 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4

# Decode / Decompress：默认 VEC=1
for d in 5 11; do F203_BYTE_DECODE_D=$d bash run.sh -r cpu -v Ascend910B4; done
for d in 5 11; do F203_DECOMPRESS_D=$d bash run.sh -r cpu -v Ascend910B4; done
```

---

## 附录 A — 探针与文档索引

| 路径 | 说明 |
|------|------|
| `pass-f203-compress-d-vec-k4/STATUS.md` | Compress 默认向量 |
| `pass-f203-decompress-d-vec-k4/STATUS.md` | Decompress 默认向量 |
| `pass-f203-byteencode-d-vec-k4/STATUS.md` | §VEC=2 实验表 |
| `pass-f203-alg6-bytedecode-d-vec-k4/STATUS.md` | d=5/11 VEC=0/1 同体 |
| `qa/2026-07/2026-07-08-Alg14-tail-pack探针.md` | §2b–§2c 讨论纪要 |
| `library/documents/CANN-AscendC算子开发接口参考-查阅索引.md` | Gather/CreateVecIndex（VEC=2 实验查阅） |
