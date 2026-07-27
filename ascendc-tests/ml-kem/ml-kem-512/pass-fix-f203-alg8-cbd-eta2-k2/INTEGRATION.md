# INTEGRATION — pass-fix-f203-alg8-cbd-eta2-k2

**语义**：FIPS 203 **Alg.8 SamplePolyCBD**，η=2，k=2（ML-KEM-512）— 独立探针 `prf_out[4,128]` → `src[4,256]`（T-B2 polyvec4）。
**分片**：P2 双 AIV — AIV0 `{0,2}`（s0,e0），AIV1 `{1,3}`（s1,e1）。

---

## 1. 与 512 后续 Encrypt 的关系

| 消费方 | 说明 |
|--------|------|
| **本探针** | W0/B3a；默认 `blockDim=2`，验证 η=2 CBD 缺项 |
| ML-KEM-512 Encrypt prep | 后续 vendored 同源；T-B2 polyvec4：r0,r1,e1,e2 等 η2 噪声行可复用同形状采样 |

维护规则：算法、布局或同步改动先在本探针 CPU+SIM 双绿，再同步后续 512 Encrypt / KEM Encaps prep 副本。

---

## 2. 默认变体

| 项 | 本探针 `run.sh` |
|----|-----------------|
| 路径 | DataCopy + `load32` + SWAR + `CBD2_AB_LUT[16]` |
| `F203_CBD_BLOCK_DIM` | **2** |
| 行表 | AIV0 `{0,2}` / AIV1 `{1,3}` |

回归对照：`CBD_TEST_BLOCK_DIM=1 bash run.sh -r sim -v Ascend910B4`。

---

## 3. Golden 与 API

- Python golden：`library/shared/fips203_se_sample/golden_se_sampling.py::sample_poly_cbd2`
- 设备 API：`F203CbdEta2::SamplePolyCbd2Batch4` / `SamplePolyCbd2Batch4WithUb`
- 布局：`src[row,256]` int32，row 0–1 = `s0,s1`，row 2–3 = `e0,e1`

AscendC API 均复用既有查阅记录：`GetBlockIdx/GetBlockNum`、`DataCopy`、`TPipe/TQue/TBuf`、`GlobalTensor/LocalTensor`、`GetValue/SetValue`、`PipeBarrier`；η2/k2 不新增 AscendC API。

---

## 4. 文档

| 文档 | 内容 |
|------|------|
| [`STATUS.md`](STATUS.md) | 验收命令、CPU/SIM 证据 |
| [`docs/specs/fips203-mlkem512-parameter-card.md`](../../../docs/specs/fips203-mlkem512-parameter-card.md) | 512 参数卡（k=2，η2=2） |
