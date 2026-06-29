# ByteEncode₁₂ 向量化方案（pass-fix-f203-2s1e-byteencode12-vec-k4）

**更新**：2026-06-15

## 目的

在 [2s1e](fix-f203-2s1e-alg13-16171820-k4/) 全链路上，将 Alg.13 行 19–20 的 `ByteEncode₁₂` 从标量改为 **UB 内向量实现**，golden 仍为 `byte_encode12_ref.c` / `gen_data.py`。

## 数据流（每 poly，tile=32 对）

```
a[256] int32（顺序系数）
  │ DataCopy 64 系数 / tile
  ▼
a_tile[64]
  │ Gather×2（字节偏移 8i / 8i+4）
  ▼
t0[32], t1[32]
  │ int32 ShiftRight/Muls/Sub/Add（12-bit 掩码与 b0/b1/b2）
  ▼
b0W[32], b1W[32], b2W[32]  (int32，值域 0..255)
  │ pack_quad12（4 pair→3 int32）+ DataCopy 96B
  ▼
r[384] → DataCopy → GM ek/sk
```

## 实现要点（已落地）

- **Gather**：`idx*8` / `+4` 字节偏移，tile=32 对。
- **位运算**：`int32` 上 `ShiftRight`/`Muls`/`Sub`/`Add`（避免 CPU sim 不支持的 `half→int32` Cast；`uint16 And` 留作后续）。
- **交织写**：`BYTE_ENCODE12_SCATTER_VEC=1` 时 `pack_quad12_i32` + `DataCopy`（**910B4 无 Scatter API**）；`0` 时标量 `scatter_b012_scalar`（`int32 & 0xFF`，勿经 `int8` 饱和）。
- **UB scratch**：+1664B（`kVecScratchBytes`），见 `bind_encode12_vec_ws`。

## 宏开关

| 宏 / CMake | 默认 | 含义 |
|------------|------|------|
| `BYTE_ENCODE12_VEC` | `1` | `0`=`poly_byte_encode12_scalar`；`1`=向量 Gather+位运算 |
| `BYTE_ENCODE12_SCATTER_VEC` | `1` | `0`=标量 `SetValue` 交织；`1`=pack+DataCopy |

环境变量与 CMake 一致：`BYTE_ENCODE12_VEC`、`BYTE_ENCODE12_SCATTER_VEC`。

## 关键文件

| 文件 | 作用 |
|------|------|
| `byte_encode12_config.hpp` | 宏默认值 |
| `byte_encode12_vec.hpp` | 向量实现 |
| `byte_encode12_pair.hpp` | 标量 + 分发入口 |
| `2s1e_post_ntt_ub.hpp` | `stageEncodeOut` + UB scratch |

## Gather 索引

```cpp
CreateVecIndex(idx, 0, 32);
Muls(idx2, idx, 8, 32);   // a[2i] 字节偏移
Gather(t0, row, idx2, 0, 32);
Adds(idx2, idx2, 4, 32); // a[2i+1]
Gather(t1, row, idx2, 0, 32);
```

post-NTT 步骤，**不受 NTT S1–S3 Gather 禁令**约束。

## 交织写（910B4：pack + DataCopy，非 Scatter）

`AscendC::Scatter` 在 `NpuArch=2201` 未实现。采用每 4 pair 打包 3 个 `int32`（12 字节小端布局），每 tile 一次 `DataCopy` 写 96 字节：

```cpp
pack_quad12_i32(packW, b0W, b1W, b2W);
DataCopy(r[byteBase], packW.ReinterpretCast<uint8_t>(), 96);
```

讨论纪要：[qa/2026-06/2026-06-15-ByteEncode12向量与Scatter讨论.md](../../qa/2026-06/2026-06-15-ByteEncode12向量与Scatter讨论.md)

## 性能（SIM，全链路 mixPass=0）

| `BYTE_ENCODE12_SCATTER_VEC` | 墙钟（约） |
|-----------------------------|------------|
| 0（标量交织） | ~54 s |
| 1（pack+DataCopy） | ~48 s |

## 验收

```bash
cd ascendc-tests/pass-fix-f203-2s1e-byteencode12-vec-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```

## 后续

- 合并回主线 2s1e（若稳定）
- `d∈{1,4,5,10,11}`：`EncodeTraits<d>` 泛化
- 行 18 **向量 basemul**（全链路下一热点，见当日 qa 纪要 §5）
