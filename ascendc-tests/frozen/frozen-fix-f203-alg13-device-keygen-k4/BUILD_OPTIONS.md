# BUILD_OPTIONS — pass-fix-f203-alg13-device-keygen-k4

**原则**：`bash run.sh` **不加 export** 即走生产路径（`ascendc_keygen_bbit` + 生产 I/O）。

**实现方案**：[`pass-fix-f203-alg13-device-keygen-k4-实现方案-customspec.tex`](pass-fix-f203-alg13-device-keygen-k4-实现方案-customspec.tex)

---

## 1. 生产入口（默认 `run.sh`）

| 项 | 默认 | 说明 |
|----|------|------|
| 二进制 | `ascendc_keygen_bbit` | `cmake/keygen` 统一构建 prep+compute |
| `RUN_MODE` | `cpu`（`-r sim` 验收 SIM） | |
| `SOC_VERSION` | `Ascend910B4` | |
| `CMAKE_BUILD_TYPE` | `Debug` | |

### 生产 I/O

| 目录 | 内容 |
|------|------|
| `input/` | `seed_d.bin` + `lut_even/odd_stacked.bin` |
| `output/` | `ek_pke.bin` + `dk_pke.bin` |

由 `scripts/prepare_production_input.py` 准备；run 结束 `_keygen_scrub_output` 删除其它 output 文件。

### 环境变量

| 变量 | 默认 | 作用 |
|------|------|------|
| `SEED_D` | `20260619` | 全链种子 |
| `KEYGEN_KERNEL_BUDGET_SEC` | `900` | SIM 墙钟预算 |
| `KEYGEN_SKIP_REBUILD` | `0` | KAT 首轮后设 `1` |
| `KEYGEN_VERIFY` | `0` | `1` → `KEYGEN_GOLDEN_ONLY=1 gen_data.py` + `verify_production.py` |
| `KEYGEN_DEBUG_DUMP` | `0` | `1` → `output/debug/` |
| `KEYGEN_KAT` | `0` | KAT 脚本静默模式 |

### SIM 环境（`run.sh` 内建，勿省略）

1. **`sim_env_export` 必须在 `_keygen_build` 之后**（build 会 `rm -rf out/`，提前建 stub 会被删 → WSL FPE）。
2. 顺序：build → `sim_env_export` → `camodel_sim_log.sh` → kernel。

---

## 2. Launch / CMake 锁定值

| CMake 宏 | 值 | 段 |
|----------|-----|-----|
| `F203_AHAT16_BLOCK_DIM` | **2** | prep |
| `F203_ALG7_REJ_IMPL` | **1** | prep |
| `F203_ALG7_D12_GATHER` | **0** | prep |
| `F203_AHAT16_BATCH_SHAKE` | **0** | prep |
| `F203_ALG7_XOF_504` | **0** | prep |
| `F203_CBD_BLOCK_DIM` | **1** | prep（canonical 算法源：[`pass-fix-f203-alg8-cbd-eta2-k4`](../pass-fix-f203-alg8-cbd-eta2-k4/)，vendored 于 `prep/alg8/`） |
| `F203_STAGE1_SPLIT` | **1** | compute |
| `F203_KEYGEN_EK_PKE` | **1** | compute |
| `HAT_ALG11_VEC` 等 | vec-k4-v2 生产默认 | compute |

Host launch：`prep blockDim=2`（AIV_ONLY）；`mmad blockDim=1`（MIX_AIC_1_2）。

---

## 3. 分段构建（非默认）

| 路径 | 产物 | I/O |
|------|------|-----|
| `cmake/prep` | `ascendc_keygen_prep_bbit` | 写 `output/a_hat,src,rho`（调试） |
| `cmake/compute` | `ascendc_kernels_bbit` | `compute_io/` staging |

**不得**作为生产示例；仅 `scripts/dev/sim_*.sh` 或历史对照。

---

## 4. 源码注释

全目录源文件含 `@probe pass-fix-f203-alg13-device-keygen-k4` 文件头。重新注入：

```bash
python3 scripts/inject_probe_code_comments.py
```

（已标注文件跳过；改 `@probe` 标记后可强制重写。）
