# pass-fix-f203-alg8-cbd-eta2-k4 — 状态

**Alg**：FIPS 203 Alg.8 `SamplePolyCBD`，η=2，k=4，batch 8 poly（4×s + 4×e 语义行）。  
**默认变体**：**P2 双 AIV**（`blockDim=2`，DataCopy + SWAR+LUT + PipeBarrier，探针默认最优路径）。  
**验收日期**：2026-06-28（`pass-` 终态，CANN 9.0.0，Ascend910B4，SEED_D=20260619）

## 一键验收（无需配置编译开关）

```bash
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```

## CPU 孪生与 AIC 日志

910B tikicpu 按 **launch blockDim** fork 进程：`blockDim=N` → 约 `N×(1 AIC + 2 AIV)` 条 `[SUCCESS]` 行，**不代表**核用了 Cube。

| launch | CPU SUCCESS 行（典型） | 说明 |
|--------|----------------------|------|
| blockDim=2 | AIC_0/1 + AIV_0..3 | 误读为 2 颗 AI Core |
| **blockDim=1（本探针 CPU 默认）** | AIC_0 + AIV_0/1 | 与 alg7 等同；AIC 空转 exit |
| SIM/NPU blockDim=2 | 无上述 tikicpu 行 | 以 `profile_aiv_log0.toml` 为准 |

P2 编译 + CPU `blockDim=1`：内核 `GetBlockNum()==1` 时 block0 串行 8 行，golden 仍全量 PASS。

## 验收

| 项 | 状态 | 说明 |
|----|------|------|
| CPU 对拍 P2（默认） | **PASS** | `bash run.sh -r cpu` |
| SIM 对拍 P2（默认） | **PASS** | `bash run.sh -r sim` |
| SIM 对拍 P1b | **PASS** | 测试 override：`CBD_TEST_BLOCK_DIM=1 bash run.sh -r sim` |

## SIM 性能（910B4，2026-06-28）

| 变体 | blockDim | Total tick | 说明 |
|------|----------|------------|------|
| **P2 双 AIV（默认）** | 2 | **18048** | 探针默认最优（2026-06-28 复跑） |
| P1b-single | 1 | **33311** | KeyGen/presample 生产锁定；`CBD_TEST_BLOCK_DIM=1` 回归 |

## 测试专用 override

| 变量 | 效果 |
|------|------|
| `CBD_TEST_BLOCK_DIM=1` | P1b-single（KeyGen 同构对照） |
| `CBD_TEST_P0_SCALAR=ON` | P0 标量 CBD |
| `CBD_TEST_P1A_SCALAR_IO=ON` | P1a SWAR+LUT + scalar GM I/O |

## 数据流

```
input/prf_out.bin [8×128 B]  →  f203_cbd_eta2_batch8  →  output/src.bin [8×256 int32]
golden: scripts/gen_data.py → output/golden_src.bin（golden_se_sampling.sample_poly_cbd2）
```

## 集成（生产与探针默认不同）

| 消费方 | blockDim | 说明 |
|--------|----------|------|
| 本探针 `run.sh` | **2** | 默认最优性能 |
| KeyGen prep | **1** | `pass-fix-f203-alg13-device-keygen-k4/prep/alg8/` vendored，单 AIV 策略 |
| presample 行 8–15 | **1** | `pass-fix-f203-alg13-lines8-15-se-k4` |

Golden：`library/shared/fips203_se_sample/golden_se_sampling.py::sample_poly_cbd2`

集成说明：[`INTEGRATION.md`](INTEGRATION.md)
