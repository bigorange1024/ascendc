# F203 — 8-poly 批 Stage123 NTT/INTT（AscendC 向量）

**锚点探针**：[`pass-fix-f203-stage123-ntt-intt-polyvec8-vec`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-stage123-ntt-intt-polyvec8-vec/)  
**数学/交付对照**：ntt_study [`sepolyvec8_ntt_f203`](../../thirdparty/ntt_onnx/examples/mlkem/deliverables/sepolyvec8_ntt_f203/)

---

## 1. 范围

| 在范围内 | 不在范围内 |
|----------|------------|
| 8×256 系数域 polyvec 批 NTT/INTT | vec-k4-v2 全 KeyGen 链（行 18 basemul、ByteEncode 等） |
| Tag5T 三段式 S1→S2→S3 | 4-poly 2s1e `[HI,Z,LO,Z]` Stage1 路径 |
| 紧凑 S0 `[HI₈,LO₈]`（16 行） | merged_kyber FSM / 交错 S0 旧路线 |

**命名**：`polyvec8` = **8 个 poly**，与 ML-KEM 方案参数 k 无关。

## 2. 计算图

```text
src[8,256] int32
  → S1  AIV×2：limb6 编码 → S0[16,256]  （每 AIV 4 poly）
  → S2  AIC：4× MMAD（even/odd × lo/hi LUT 半区）
  →     AIV×2：mat_c 平面 pack
  → S3  AIV×2：平面 merge + stage31 mod → dst[8,256]
```

NTT / INTT：**同一图**，host 写入不同 LUT（`kMlkemLimb6Ntt_T_i8` / `kMlkemLimb6Intt_T_i8`）。

## 3. 设备资源

| 项 | 值 |
|----|-----|
| `blockDim` | **1** |
| 子核 | **1×AIC + 2×AIV**（`KERNEL_TYPE_MIX_AIC_1_2`） |
| launch / 次 run | **1** |
| CrossCore | S1 完成 → AIC MMAD；S2 完成 → AIV pack |

## 4. 验收与性能（910B4 SIM，mixPass=3）

| 模式 | CPU | SIM tick |
|------|-----|----------|
| NTT | PASS | **30347** |
| INTT | PASS | **30340** |

正确性：设备 vs golden **max=0**；vs ntt_study C（Tag5T / F203 Tag3）**max=0**。

独立 C 对拍：`python3 scripts/cross_check_ntt_study_c.py --regen`（不接入 `run.sh`）。

## 5. 与相邻用例

| 用例 | 关系 |
|------|------|
| [`pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/) | 代码 fork 源；4-poly 2s1e 全链 |
| [`exp-sepolyvec8-ntt-k8`](../../examples/incubating/ml-kem/ml-kem-1024/exp-sepolyvec8-ntt-k8/) | 历史 exp；交错 S0；本探针为 **紧凑 polyvec8** 向量终态 |
| `sepolyvec8_ntt_f203` | ONNX/实机交付 golden；C 语义同源 |

## 6. 运行

```bash
cd ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-stage123-ntt-intt-polyvec8-vec
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
F203_NTT_MODE=intt bash run.sh -r sim -v Ascend910B4
```
