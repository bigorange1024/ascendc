# pass-fix-f203-2s1e-byteencode12-vec-k4

**ByteEncode₁₂-only** probe（k=4，2×AIV 各编码 2 poly → ek/sk）。

## 输入契约（与 Alg.13 vec-k4-v2 mixPass=7 preset **字节级一致**）

| 文件 | 形状 | 集成对应 |
|------|------|----------|
| `input/dst.bin` | `[12,256]` int32 | `dst_preset` / `golden.bin`（NTT 后 ŝ‖ê 双块布局） |
| `input/t_hat.bin` | `[4,256]` int32 | `t_hat_preset` / `golden_t_hat.bin`（行 18 后 t̂） |
| `output/ek_polyvec.bin` | 1536 B | `ByteEncode₁₂(t̂)` |
| `output/sk_polyvec.bin` | 1536 B | `ByteEncode₁₂(ŝ)`，ŝ=dst[0:4] |

Golden 链：**复用 v2 `gen_data.py`**（src→NTT→hat→byte_encode12_ref），非独立随机 fixture。

设备读入与 `byte_encode12_only.hpp` / 集成 `loadNttPresetInto` + `loadThatPreset` 相同。

## 宏

| 宏 | 默认 | 说明 |
|----|------|------|
| `BYTE_ENCODE12_VEC` | 1 | 0=标量 / 1=向量 |
| `BYTE_ENCODE12_SCATTER_VEC` | 1 | 0=SetValue / 1=pack+DataCopy |
| `BYTE_ENCODE12_PREFETCH` | 1 | 0=tile32 循环+每 tile CreateVecIndex；**1=整 poly ROM Gather×1+128-wide 算** |

## 测试

```bash
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
# 对照 legacy tile 路径：
BYTE_ENCODE12_PREFETCH=0 bash run.sh -r sim -v Ascend910B4
```

## 测试结果（Ascend910B4，Alg.13 真实输入）

| 配置 | CPU | SIM Total tick |
|------|-----|----------------|
| `PREFETCH=1`（默认） | PASS | **17429** |
| `PREFETCH=0`（tile32） | PASS | **25464** |

prefetch 相对 tile32：**−32% tick**（17429 vs 25464）。v2 全链路边际 encode：**+12421**（见 [vec-k4-v2/SIM_BENCHMARK.md](../pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/SIM_BENCHMARK.md)）。

## 集成替换

可直接替换 `byte_encode12_vec.hpp` 中 `poly_byte_encode12_local` 分发；`stageEncodeOut` 调用约定不变（`LocalTensor` 上的 poly 视图 + 同一 scratch 槽）。

**Encrypt 扩展**：计划在本探针上支持 `F203_BYTE_ENCODE_D=4|10`（不新建目录），见 [`BYTE_ENCODE12_VEC.md`](BYTE_ENCODE12_VEC.md) §多 d 扩展。
