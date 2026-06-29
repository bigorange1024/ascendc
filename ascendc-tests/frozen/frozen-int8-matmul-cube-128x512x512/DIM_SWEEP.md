# 降维扫参记录（基于本用例 + MATMUL_* 环境变量）

运行：`KERNEL_COMPUTE_BUDGET_SEC=15 bash scripts/dim_sweep.sh`

## 结论（2026-06-11）

### Phase A：单核 `blockDim=1, usedCoreNum=1, single=(M,N,K)`

| 形状 | 结果 |
|------|------|
| 128×512×512 | **TIMEOUT**（无 output.bin） |
| 16×256×512 | **TIMEOUT** |

→ CPU tikicpulib 上 **`Matmul::IterateAll` 单核全吃** 当前不可用，与 M/N/K 无关（至少到 128 仍挂）。

### Phase B：多核降 M（`singleCoreM=16, singleCoreN=128`，`blockDim=usedCoreNum/2`）

| 形状 | blockDim | usedCoreNum | 结果 |
|------|----------|-------------|------|
| 128×512×512 | 16 | 32 | **PASS** |
| 64×512×512 | 8 | 16 | **PASS** |
| 32×512×512 | 4 | 8 | **PASS** |
| 16×512×512 | 2 | 4 | **PASS** |

### Phase C：Kyber Stage2 原生 **16×256×512**

| 配置 | 结果 |
|------|------|
| `blockDim=1, usedCoreNum=2, single=(16,256)` | **PASS** ✅ |
| `blockDim=2, usedCoreNum=4, single=(16,128)` | **PASS** ✅ |
| `blockDim=2, usedCoreNum=2, single=(16,256)` | **TIMEOUT** ❌ |

**无需垫成 128×512×512**。f203 风格 `aicore=1`（`blockDim=1, SetDim(2), singleCoreN=256`）在 CPU 上可跑通原生 16×256×512。

## 跑 Kyber Stage2 原生形状

```bash
export MATMUL_M=16 MATMUL_N=512 MATMUL_K=256
export MATMUL_BLOCK_DIM=1 MATMUL_USED_CORE_NUM=2
export MATMUL_SINGLE_M=16 MATMUL_SINGLE_N=256
KERNEL_COMPUTE_BUDGET_SEC=15 bash run.sh -r cpu -v Ascend910B4
```

## 默认 `run.sh`

未设置 `MATMUL_*` 时默认为 **单核 128×512×512**（会挂）。多核 128 或 Kyber 16 需显式 export 上述变量。
