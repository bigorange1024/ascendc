# IMPLEMENTATION_PLAN — pass-f203-byteencode-d4-d10-vec-k4

**状态**：**pass**（A0：d=**4** / d=**10** CPU+SIM PASS）  
**验收 d**：**`{4, 10}`** only  
**FIPS 203**：Algorithm 5 `ByteEncode_d`；Alg.14 行 22–23 后半部  
**参考**：mlkem-native `compress.c` 打包布局；向量模式参考 [`pass-fix-f203-2s1e-byteencode12-vec-k4`](../pass-fix-f203-2s1e-byteencode12-vec-k4/)（**只读，不修改**）

---

## 1. 规范锚点

输入：256×int32，各系数 ∈ `[0, 2^d-1]`（通常来自 `Compress_d` 输出）。

| d | Encrypt 用途 | 输出字节/poly |
|---|--------------|---------------|
| 10 | `ByteEncode_10(Compress_10(u))` → c₁ | 320 |
| 4 | `ByteEncode_4(Compress_4(v))` → c₂ | 128 |

布局对齐 mlk `mlk_poly_compress_d4_c` / `mlk_poly_compress_d10_c` 的 **比特打包半部**（不含 Barrett 压缩）。

---

## 2. 目录骨架

```text
pass-f203-byteencode-d4-d10-vec-k4/
├── byte_encode_d_ref.c/h      # Host golden
├── byte_encode_d_vec.hpp      # 向量 mask + 分组 pack
├── byte_encode_d_custom.cpp   # AIV-only
├── run.sh / scripts/
```

**与 byteencode12 探针分工**：

- [`pass-fix-f203-2s1e-byteencode12-vec-k4`](../pass-fix-f203-2s1e-byteencode12-vec-k4/)：**d=12** KeyGen 专用（2×AIV preset ŝ‖ê + t̂），保持 frozen PASS 路径。
- **本目录**：Encrypt 侧 **d=4 / d=10** 单 poly 探针；CMake `-DF203_BYTE_ENCODE_D=4|10`。

---

## 3. AscendC 向量策略

1. **向量**：`mask_low_bits_i32`（`ShiftRight`+`Muls`+`Sub`，256-wide）— 复用 compress/byteencode12 模板。
2. **pack**：跨字节边界 nibble / 10-bit 交织 → 分组 **标量 SetValue**（32 组 d=4 / 64 组 d=10）；910B 无 Scatter，与 byteencode12 pack 策略一致。

---

## 4. 衔接

- 上游：[`pass-f203-compress-d4-d10-vec-k4`](../pass-f203-compress-d4-d10-vec-k4/) 输出 comp 域
- 下游 decode：[`pass-f203-alg6-bytedecode-d4-d10-vec-k4`](../pass-f203-alg6-bytedecode-d4-d10-vec-k4/)

---

## 5. 验收

```bash
F203_BYTE_ENCODE_D=4  bash run.sh -r cpu -v Ascend910B4
F203_BYTE_ENCODE_D=4  bash run.sh -r sim -v Ascend910B4
F203_BYTE_ENCODE_D=10 bash run.sh -r cpu -v Ascend910B4
F203_BYTE_ENCODE_D=10 bash run.sh -r sim -v Ascend910B4
```
