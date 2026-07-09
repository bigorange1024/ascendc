# F203 NTT 域内积（polyvec）— 技术总结

**读者**：实现 Alg.13 行 18 类 \(\widehat{A}\cdot\hat{s}\) 的开发者  
**目的**：说明 **NTT 域矩阵–向量乘** 的数学、布局契约与实现模式；非函数名踩坑表  
**活跃探针**：`pass-fix-f203-alg11-12-innerproduct-k4`、`innerproduct-k4-halfrows`  
**融合**：[F203-2s1e-NTT内积UB融合技术总结.md](F203-2s1e-NTT内积UB融合技术总结.md)  
**讨论**：[qa/2026-06/2026-06-17-…](../../qa/2026-06/2026-06-17-innerproduct-k4一二期路线讨论.md)、[qa/2026-06/2026-06-18-…](../../qa/2026-06/2026-06-18-内积布局与NTT内积UB融合讨论.md)

---

## 0. 本文怎么读

| 章节 | 内容 |
|------|------|
| §1 | 数学：NTT 域内积 |
| §2 | 布局契约（与 \(K\) 无关的约定） |
| §3 | 实现模式：全 poly vs half 批处理 |
| §4 | UB / TQue 资源原则 |
| §5 | 验证金字塔 |
| §6 | 附录：二期冻结、tick、案例 |

---

## 1. 数学

对输出行 \(p\in[0,P)\)、列 \(j\in[0,S)\)（KeyGen \(K=4\)）：

\[
\hat{t}[p] = \mathrm{mod}_q\left(\sum_{j=0}^{S-1} \widehat{A[p,j]} \circ \hat{s}[j]\right)
\]

- \(\circ\) = **NTT 域多项式乘**（长度 \(N\)，paired basemul + 累加）  
- \(\mathrm{mod}_q\)：可对 \(j\) **int64 累加后一次 mod**（\(S\) 小）  
- **加 \(\hat{e}\)** 是行 18 的扩展：先验证无 ê 的 \(\widehat{A}\cdot\hat{s}\)，再加噪声  

与 [F203-2s1e-NTT内积UB融合技术总结.md](F203-2s1e-NTT内积UB融合技术总结.md) §1 同构。

---

## 2. 布局契约

### 2.1 行主序（唯一）

\[
\mathrm{flat}(p,j,c) = (p\cdot K + j)\cdot N + c
\]

**不变量**：golden、`gen_data`、GM 读 kernel、alg13 融合 **同一公式**。  
**禁止** 为「合并 DataCopy」引入列主序 `a_col` 而又在融合侧用行主序 — 会导致单探针 PASS、融合 FAIL。

### 2.2 多核

- **全 poly 单 AIV**：`blockDim=1`，最简单对拍  
- **半行双 AIV**：按输出行 \(p\) 分片；每核仍握 **完整** \(\hat{s}[j]\)  

---

## 3. 实现模式

### 3.1 P-inner-1：全 poly 累加（活跃默认）

```
for j in 0..S-1:
  load ŝ[j] once
  for p in 0..P-1:
    acc[p] += MultiplyNTTs(Â[p,j], ŝ[j])
mod_q(acc[p])
```

| 优点 | 缺点 |
|------|------|
| 循环简单、UB 地图清晰 | 每次 basemul 搬整 poly \(N\) |
| 小 \(P,S\) 下 SIM 已够快 | \(P\cdot S\) 大时 GM 读次数增 |

**判决**：\(4\times4\times1\) 下与 half 批处理 **打平** → 保留全 poly，冻结 half 路线。

### 3.2 P-inner-2：half 批处理（已冻结）

在单 AIV 内把 \(N\) 切成 \(2\times128\)，缓存 \(\hat{s}\) 的 b-lane，减少 GM 读。

**冻结原因**（原理）：小形状下 **维护 half 外层 + scratch 地图** 的成本抵消 GM 节省；复杂度高、踩坑密度大。  
**教训**：优化前先估 **算术强度 vs 管理开销**；见 [研究路线与frozen治理.md](研究路线与frozen治理.md)。

**禁止** 从 `frozen-fix-f203-alg11-12-innerproduct-k4-halfbatch` 抄码。

---

## 4. UB / TQue 原则

| 规则 | 说明 |
|------|------|
| basemul 输入经 TQue 握手 | 与 Alg11 单 tile 同构 |
| ROM 尺寸 = \(N/2\) pair | 不借用 half-NTT 的 \(N/4\) |
| scratch 大块用 TBuf；冲突用 **独立 TQue** | half 路线曾 scratch 重叠静默错 |
| VECIN TQue ≤ 8 | [ascendc-TQue与Pipe框架知识库.md](ascendc-TQue与Pipe框架知识库.md) |

---

## 5. 验证金字塔

```
Gate-0: multiplyntts-k4 单 tile     → golden_∘
Gate-1: innerproduct 无 ê             → golden_t_hat_dot
Gate-2: alg13 mixPass=4 内积-only     → 同 golden
Gate-3: 融合 mixPass=0                → 见 2s1e 总结
```

---

## 6. 附录

### 6.1 二期冻结

| 项 | 内容 |
|----|------|
| 目录 | `frozen/frozen-fix-f203-alg11-12-innerproduct-k4-halfbatch` |
| 继任 | `ProcessFullPoly` only |

### 6.2 性能（4×4×1，910B4 SIM，示意）

| 路线 | tick |
|------|------|
| 全 poly 单 AIV | **43992** |
| 半行双 AIV | **26185** |
| 独立内积探针（halfrows，2026-06-19） | **26185** |

布局统一后全量 tick 略升（逐行读 `a_hat`）；**正确性优先**。

### 6.3 命令

```bash
cd ascendc-tests/pass-fix-f203-alg11-12-innerproduct-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```

### 6.4 集成 checklist

- [ ] 与 alg13 **同一 `a_hat.bin`**
- [ ] `mixPass=4` 内积-only 通过后再融合
- [ ] `+ê` 单独 golden 层

---

*原 `F203-innerproduct-k4-一二期技术总结.md`；2026-06-18 重构并更名。*
