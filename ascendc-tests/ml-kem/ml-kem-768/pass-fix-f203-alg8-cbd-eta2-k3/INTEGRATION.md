# INTEGRATION — pass-fix-f203-alg8-cbd-eta2-k3

**语义**：FIPS 203 **Alg.8 SamplePolyCBD**，η=2，k=3（ML-KEM-768）— 独立探针 `prf_out[6,128]` → `src[6,256]`（polyvec6）。  
**分片**：P2 双 AIV — AIV0 `{0,1,3}`（s0,s1,e0），AIV1 `{2,4,5}`（s2,e1,e2）。

---

## 1. 与 KeyGen / 下游关系

| 消费方 | 说明 |
|--------|------|
| **本探针** | 本目录；blockDim=**2**（SIM/NPU 默认最优） |
| ML-KEM-768 KeyGen prep | 后续 vendored 同源；P1b 单 AIV（`CBD_TEST_BLOCK_DIM=1`） |

**维护规则**：算法/性能改动先在**本探针**验收，再同步 768 KeyGen prep 副本。

---

## 2. 默认变体

| 项 | 本探针 `run.sh` |
|----|-----------------|
| 路径 | P2 DataCopy + SWAR+LUT |
| `F203_CBD_BLOCK_DIM` | **2** |
| 行表 | AIV0 `{0,1,3}` / AIV1 `{2,4,5}` |

P1b 回归：`CBD_TEST_BLOCK_DIM=1 bash run.sh -r sim`。

---

## 3. Golden 与 API

- Golden：`library/shared/fips203_se_sample/golden_se_sampling.py::sample_poly_cbd2`
- 设备 API：`F203CbdEta2::SamplePolyCbd2Batch6` / `SamplePolyCbd2Batch6WithUb`
- 布局：`src[row,256]` int32，row 0–2 = ŝ，3–5 = ê（polyvec6）

---

## 4. 文档

| 文档 | 内容 |
|------|------|
| [`STATUS.md`](STATUS.md) | 验收命令、CPU/SIM 证据 |
| [`docs/specs/fips203-mlkem768-parameter-card.md`](../../../docs/specs/fips203-mlkem768-parameter-card.md) | 768 参数卡（T-B polyvec6） |
