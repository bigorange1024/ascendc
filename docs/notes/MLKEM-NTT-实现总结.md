# ML-KEM Tag5T NTT — 数学契约与技术总结

**读者**：实现 FIPS 203 / ML-KEM 正向 NTT 的开发者（未读过本仓库亦可读 §1–§4）  
**目的**：定义 **poly-batch 三段式的数学与数据契约**；设备实现见 [MLKEM-NTT-向量与标量实现指南.md](MLKEM-NTT-向量与标量实现指南.md)  
**活跃探针**：`pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2`（向量全链路）；`exp-mlkem-f203-alg13-16171820-2s1e-k4`（预研）  
**讨论**：[qa/2026-06/2026-06-12-…§7](../../qa/2026-06/2026-06-12-F203-alg13行18-TQue与模运算讨论.md#7-mlkem-ntt-poly-batch-架构定稿同日追加)

---

## 0. 本文怎么读

| 章节 | 依赖代码名 |
|------|------------|
| §1 NTT 三段式在算什么 | 否 |
| §2 poly-batch 核心约束 | 否 |
| §3 数据契约（\(k=8\) 例） | 少量 |
| §4 路线对照与 frozen 含义 | 是（附录性质） |
| §5 下游 Alg.13 衔接 | 否 |
| §6 案例索引 | 是 |

---

## 1. 数学：Tag5T 三段式

对模 \(q\)（Kyber \(q=3329\)）长度 \(N=256\) 的多项式，本仓采用 **编码 → 矩阵乘 → 重组取模**：

\[
\hat{f} = \mathrm{RouteA\_mod}\bigl(\mathrm{MMAD}(\mathrm{Encode}(f), M_0), \mathrm{MMAD}(\mathrm{Encode}(f), M_1)\bigr)
\]

| 阶段 | 作用 |
|------|------|
| **Stage1** | int32 系数 → **limb 编码**（6 bit 等）→ int8 矩阵行 |
| **Stage2** | 与 **竖堆 LUT** 做两次 int8 MMAD → 中间 int32 |
| **Stage3** | **RouteA** 重组 limb + **mod \(q\)** → NTT 域系数 |

Golden 对齐 FIPS / `ntt_study` 的 `MlkemNtt`，**不等于** merged_kyber 的 Barrett Merge 路径。

---

## 2. 核心架构约束（poly-batch）

以下约束是 **数学/并行语义**，与具体类名无关。

### 2.1 完整 poly 驻留单 AIV

| 必须 | 禁止 |
|------|------|
| Stage2 后，**每个 AIV 在 UB 握有若干完整 poly 的 hi+lo** | 单 poly 的 hi 在 AIV0、lo 在 AIV1（「limb 面对半」Stage3） |
| Stage3 在 **单 AIV 内** 完成 merge + mod | **NTT S1–S3 内** 用 Gather 拼半 poly、AIV 间 GM 交换 |

### 2.2 Stage1 按 poly 批切分

- 按 **`kPolysPerAiv`** 分配 poly 行，每 poly **整行 256 系数** 做 limb 编码  
- **禁止** 按系数半维切分（`subCoreIdx×128`），使同一 poly 的 hi/lo 分到不同 AIV

### 2.3 数学等价性

poly-batch 与「limb 面对半」布局在 **相同输入 poly** 下，\(\hat{f}[i,:]\) 逐系数一致；差异仅在 **中间张量布局与搬运**。

---

## 3. 数据契约（\(k=8\) poly-batch 参考）

```text
src [8,256] int32
lut_stacked [512,256] int8   — Tag5T 竖堆 LUT

Stage1 → S0 [16,256] int8
  行序：[hi0..hi3, lo0..lo3, hi4..hi7, lo4..lo7]
  AIV0 → 行 0..7；AIV1 → 行 8..15

Stage2 → mat_c [32,256] int32（竖堆）
  C_lo 行 0..15；C_hi 行 16..31

Stage2 后读 GM、UB 拼完整 poly：
  AIV0：C_lo[0:8) + C_hi[16:24)  → 4 poly × (128 lo + 128 hi)
  AIV1：C_lo[8:16) + C_hi[24:32)

Stage3 → dst [8,256] int32
```

**行主序**：\(\mathrm{flat}(p,c)=p\cdot N+c\)。换 \(k\) 时行数按 \(2k\) 缩放，**结构不变**。

**活跃 2s1e 变体**：平面 `mat_c [96,128]`、NTT S1–S3 **无 Gather** — 见实现指南；**数学输出契约**与上式 dst 一致。

---

## 4. 路线对照（读判决，不抄码）

| 路线 | 地位 | Stage3 语义 |
|------|------|-------------|
| **2s1e 集成** | **活跃** | 平面 mat_c；无 Gather |
| Tag5T poly-batch | frozen；契约对照 | 竖堆 mat_c |
| Tag5T limb 面对半 | frozen | hi/lo 分核 |
| merged_kyber A0/A1 | frozen | Barrett ≠ RouteA |
| Matmul RouteA 宽表 | **废弃** | Gather 路线 |

索引：[ascendc-tests/frozen/INDEX.md](../../ascendc-tests/frozen/INDEX.md)、[研究路线与frozen治理.md](研究路线与frozen治理.md)。

---

## 5. 与 Alg.13（行 16–20）的衔接

| 原则 | 说明 |
|------|------|
| NTT 产出 \(\hat{s},\hat{e}\) 后 | 行 18 为 NTT 域 **乘加** + mod，不是 invNTT |
| 行 18 不需 `SHAT_PEER` | 每 AIV 本地完整 \(\hat{s}\) |
| 融合时 | 中间态 **UB 驻留**；见 [F203-2s1e-NTT内积UB融合技术总结.md](F203-2s1e-NTT内积UB融合技术总结.md) |

---

## 6. 附录

### 6.1 验收

```bash
cd ascendc-tests/frozen/frozen-fix-f203-2s1e-alg13-16171820-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```

| 检查 | 期望 |
|------|------|
| `mixPass=0` | S0、mat_c、dst `max_abs_diff=0` |
| vs `MlkemNtt`（C） | `max_abs_diff=0` |

### 6.2 相关文档

- [MLKEM-NTT-向量与标量实现指南.md](MLKEM-NTT-向量与标量实现指南.md)
- [ascendc-DataCopy与数据搬运知识库.md](ascendc-DataCopy与数据搬运知识库.md)
- [ascendc-TQue与Pipe框架知识库.md](ascendc-TQue与Pipe框架知识库.md)

---

*2026-06-18：重构为原理优先；原「实现总结」定稿日 2026-06-12。*
