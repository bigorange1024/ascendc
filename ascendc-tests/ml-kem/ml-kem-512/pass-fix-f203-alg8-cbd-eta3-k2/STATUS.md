# pass-fix-f203-alg8-cbd-eta3-k2 — 状态

**Alg**：FIPS 203 Alg.8 `SamplePolyCBD`，η=3，k=2（ML-KEM-512），batch 4 poly（T-B2 polyvec4：s0,s1,e0,e1）。  
**默认变体**：P2 双 AIV（`blockDim=2`，DataCopy + load24/SWAR+LUT + PipeBarrier）。  
**分片**：AIV0 `{0,2}`（s0,e0）/ AIV1 `{1,3}`（s1,e1）；`ROWS_PER_AIV=2`。  
**核符号**：`f203_cbd_eta3_batch4` / `SamplePolyCbd3Batch4`。  
**验收日期**：2026-07-27（CANN 9.0.0，Ascend910B4，SEED_D=20260619，Cloud）。

## 一键验收（无需配置编译开关）

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

## CPU 孪生与 AIC 日志

CPU 孪生路径固定 launch `blockDim=1`；默认设备代码仍编译为 P2，运行时 `GetBlockNum()==1` 时 block0 串行 4 行，golden 仍全量 PASS。SIM/NPU 默认 `blockDim=2`，以 `profile_aiv_log0.toml` 与 `Total tick` 为准。

## 验收

| 项 | 状态 | 说明 |
|----|------|------|
| CPU 对拍 P2（默认） | **PASS** | `bash run.sh -r cpu -v Ascend910B4` → `[verify] src[4,256] PASS (4096 bytes exact match)` |
| SIM 对拍 P2（默认） | **PASS** | `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` → verify PASS；**Total tick: 13566**；用例根无 stray dump |

## SIM 性能（910B4）

| 变体 | blockDim | Total tick | 说明 |
|------|----------|------------|------|
| P2 双 AIV（默认） | 2 | **13566** | polyvec4 / ROWS=4 / η3 |

## 测试专用 override

| 变量 | 效果 |
|------|------|
| `CBD_TEST_BLOCK_DIM=1` | P1b-single |
| `CBD_TEST_P0_SCALAR=ON` | P0 标量 CBD |
| `CBD_TEST_P1A_SCALAR_IO=ON` | P1a SWAR+LUT + scalar GM I/O |

## 数据流

```text
input/prf_out.bin [4×192 B]  →  f203_cbd_eta3_batch4  →  output/src.bin [4×256 int32]
golden: scripts/gen_data.py → output/golden_src.bin（golden_se_sampling.sample_poly_cbd3）
```

Golden：`library/shared/fips203_se_sample/golden_se_sampling.py::sample_poly_cbd3`；共享 C 对照：`fips203_sample_poly_cbd3_row`。  
集成说明：[`INTEGRATION.md`](INTEGRATION.md)
