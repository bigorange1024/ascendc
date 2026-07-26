# pass-fix-f203-alg8-cbd-eta2-k3 — 状态

**Alg**：FIPS 203 Alg.8 `SamplePolyCBD`，η=2，k=3（ML-KEM-768），batch 6 poly（polyvec6：3×s + 3×e）。  
**默认变体**：**P2 双 AIV**（`blockDim=2`，DataCopy + SWAR+LUT + PipeBarrier）。  
**分片**：AIV0 `{0,1,3}`（s0,s1,e0）/ AIV1 `{2,4,5}`（s2,e1,e2）；`ROWS_PER_AIV=3`。  
**核符号**：`f203_cbd_eta2_batch6` / `SamplePolyCbd2Batch6`。  
**验收日期**：2026-07-26（CANN 9.0.0，Ascend910B4，SEED_D=20260619，Cloud）

## 一键验收（无需配置编译开关）

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

## CPU 孪生与 AIC 日志

910B tikicpu 按 **launch blockDim** fork 进程：`blockDim=N` → 约 `N×(1 AIC + 2 AIV)` 条 `[SUCCESS]` 行，**不代表**核用了 Cube。

| launch | CPU SUCCESS 行（典型） | 说明 |
|--------|----------------------|------|
| **blockDim=1（本探针 CPU 默认）** | AIC_0 + AIV_0/1 | AIV_ONLY；AIC 空转 exit |
| SIM/NPU blockDim=2 | 无上述 tikicpu 行 | 以 `profile_aiv_log0.toml` 为准 |

P2 编译 + CPU `blockDim=1`：内核 `GetBlockNum()==1` 时 block0 串行 6 行，golden 仍全量 PASS。

## 验收

| 项 | 状态 | 说明 |
|----|------|------|
| CPU 对拍 P2（默认） | **PASS** | `bash run.sh -r cpu -v Ascend910B4` → `[verify] src[6,256] PASS (6144 bytes)` exit 0 |
| SIM 对拍 P2（默认） | **PASS** | `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` → verify PASS；**Total tick: 14949**；用例根无 stray dump |

## SIM 性能（910B4，2026-07-26）

| 变体 | blockDim | Total tick | 说明 |
|------|----------|------------|------|
| **P2 双 AIV（默认）** | 2 | **14949** | polyvec6 / ROWS=6 |

## 测试专用 override

| 变量 | 效果 |
|------|------|
| `CBD_TEST_BLOCK_DIM=1` | P1b-single |
| `CBD_TEST_P0_SCALAR=ON` | P0 标量 CBD |
| `CBD_TEST_P1A_SCALAR_IO=ON` | P1a SWAR+LUT + scalar GM I/O |

## 数据流

```
input/prf_out.bin [6×128 B]  →  f203_cbd_eta2_batch6  →  output/src.bin [6×256 int32]
golden: scripts/gen_data.py → output/golden_src.bin（golden_se_sampling.sample_poly_cbd2）
```

Golden：`library/shared/fips203_se_sample/golden_se_sampling.py::sample_poly_cbd2`

集成说明：[`INTEGRATION.md`](INTEGRATION.md)
