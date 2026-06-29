# INTEGRATION — pass-fix-f203-alg8-cbd-eta2-k4

**语义**：FIPS 203 **Alg.8 SamplePolyCBD**，η=2，k=4 — 独立探针 `prf_out[8,128]` → `src[8,256]`。  
**验收**：2026-06-28 CPU+SIM PASS（默认 P2 双 AIV，零配置 `bash run.sh`）。

---

## 1. 与 KeyGen / presample 关系

| 消费方 | 源码位置 | blockDim | 说明 |
|--------|----------|----------|------|
| **本探针** | 本目录 | **2**（SIM/NPU 默认最优） | 性能参考与 Pipe 细同步闭环 |
| KeyGen prep | [`pass-fix-f203-alg13-device-keygen-k4/prep/alg8/`](../pass-fix-f203-alg13-device-keygen-k4/prep/alg8/) | **1** | vendored 同源；`SamplePolyCbd2Batch8WithUb` 在 block0 |
| presample 行 8–15 | [`pass-fix-f203-alg13-lines8-15-se-k4`](../pass-fix-f203-alg13-lines8-15-se-k4/) | **1** | CMake `_SE_ALG8_INC` → 本目录（include 头文件） |

**维护规则**：算法/性能改动先在**本探针**验收，再同步 vendored 副本与 [`PIPE_SYNC_EVAL.md`](../pass-fix-f203-alg13-device-keygen-k4/PIPE_SYNC_EVAL.md) §4。

---

## 2. 默认变体（探针 vs 生产）

| 项 | 本探针 `run.sh` | KeyGen / presample |
|----|-----------------|---------------------|
| 路径 | P2 DataCopy + SWAR+LUT | P1b-single |
| `F203_CBD_BLOCK_DIM` | **2** | **1** |
| SIM tick（910B4） | **~18048** | prep 段含于 ~806k |

P1b 回归：`CBD_TEST_BLOCK_DIM=1 bash run.sh -r sim`（tick ~33311）。

---

## 3. Golden 与 API

- Golden：`library/shared/fips203_se_sample/golden_se_sampling.py::sample_poly_cbd2`
- 设备 API：`F203CbdEta2::SamplePolyCbd2Batch8` / `SamplePolyCbd2Batch8WithUb`
- 布局：`src[row,256]` int32，row 0–3 = ŝ，4–7 = ê（与 vec-k4-v2 `src.bin` 一致）

---

## 4. 文档

| 文档 | 内容 |
|------|------|
| [`STATUS.md`](STATUS.md) | 验收命令、SIM tick、CPU 孪生 AIC 日志说明 |
| [`docs/notes/F203-CBD-eta2-性能优化技术总结.md`](../../docs/notes/F203-CBD-eta2-性能优化技术总结.md) | P0–P2 路线与 KeyGen 关系 |
| [`qa/2026-06/2026-06-22-Alg8-CBD-eta2-性能优化讨论.md`](../../qa/2026-06/2026-06-22-Alg8-CBD-eta2-性能优化讨论.md) | 讨论补录 |
