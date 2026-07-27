# IMPLEMENTATION_PLAN — pass-f203-byteencode-d-vec-k4

**状态**：**pass**（d=**4/5/10/11** CPU+SIM PASS）  
**目录更名**（2026-07-08）：`pass-f203-byteencode-d4-d10-vec-k4` → **`pass-f203-byteencode-d-vec-k4`**（与 `pass-f203-compress-d-vec-k4` 命名对齐；验收 d 由 `{4,10}` 扩至 `{4,5,10,11}`）。  
**FIPS 203**：Algorithm 5 `ByteEncode_d`；Alg.14 行 22–23 后半部  
**参考**：mlkem-native `compress.c` 打包布局；向量模式参考 [`pass-fix-f203-2s1e-byteencode12-vec-k4`](../pass-fix-f203-2s1e-byteencode12-vec-k4/)（**只读，不修改**）

---

## 1. 规范锚点

输入：256×int32，各系数 ∈ `[0, 2^d-1]`（通常来自 `Compress_d` 输出）。

| d | Encrypt 用途 | 输出字节/poly |
|---|--------------|---------------|
| 11 | ML-KEM-1024 `ByteEncode_11(Compress_11(u))` → c₁ | 352 |
| 10 | ML-KEM-512/768 c₁ | 320 |
| 5 | ML-KEM-1024 c₂ | 160 |
| 4 | ML-KEM-512/768 c₂ | 128 |

布局对齐 mlk `mlk_poly_compress_d*` 的 **比特打包半部**（不含 Barrett/cast_div 压缩本体）。

---

## 2. 目录骨架

```text
pass-f203-byteencode-d-vec-k4/
├── byte_encode_d_ref.c/h      # Host golden（d=4/5/10/11）
├── byte_encode_d_vec.hpp      # 向量 mask + 分组 pack
├── byte_encode_d_custom.cpp   # AIV-only
├── run.sh / scripts/
```

**与 byteencode12 探针分工**：

- [`pass-fix-f203-2s1e-byteencode12-vec-k4`](../pass-fix-f203-2s1e-byteencode12-vec-k4/)：**d=12** KeyGen 专用。
- **本目录**：Encrypt 侧 **d=4/5/10/11** 单 poly；CMake `-DF203_BYTE_ENCODE_D=4|5|10|11`。

---

## 3. AscendC 向量策略

| d | 分组 | pack 循环 |
|---|------|-----------|
| 4 | 8 coeff → 4B | 32 |
| 5 | 8 coeff → 5B | 32 |
| 10 | 4 coeff → 5B | 64 |
| 11 | 8 coeff → 11B | 32 |

1. **向量**：`mask_low_bits_i32`（256-wide）。
2. **pack**：跨字节边界交织 → 分组 **标量 SetValue**（与 d=4/10 同策略；**非** Alg.5 逐比特标量环）。

---

## 4. 衔接

- 上游：[`pass-f203-compress-d-vec-k4`](../pass-f203-compress-d-vec-k4/)
- 下游 decode：[`pass-f203-alg6-bytedecode-d-vec-k4`](../pass-f203-alg6-bytedecode-d-vec-k4/)
- tail pack 抄码：[`pass-fix-f203-alg14-lines20-22-23-24-encrypt-pack-k4`](../pass-fix-f203-alg14-lines20-22-23-24-encrypt-pack-k4/)

---

## 5. 验收

```bash
for d in 4 5 10 11; do
  F203_BYTE_ENCODE_D=$d bash run.sh -r cpu -v Ascend910B4
  F203_BYTE_ENCODE_D=$d SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
done
```
