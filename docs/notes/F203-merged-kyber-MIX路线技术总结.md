# F203 merged_kyber MIX 路线 — 技术总结

**读者**：实现 Kyber/ML-KEM 类 MIX 三段式 NTT 的 Agent / 开发者  
**目的**：说明 **merged_kyber FSM + AicMmad** 路线的工程原理与已关闭分支，而非罗列探针目录表  
**状态**：本路线 **已冻结**（继任为 Tag5T / 2s1e）；本文供理解历史决策与禁止复活的模式  
**讨论**：[qa/2026-06/2026-06-10-F203-MIX-merged_kyber路线与limb6.md](../../qa/2026-06/2026-06-10-F203-MIX-merged_kyber路线与limb6.md)、[qa/2026-06/2026-06-11-…#NTT-Matmul](../../qa/2026-06/2026-06-11-ascendc-engineering-notes与数据搬运.md#ntt-matmul路线废弃冻结)  
**关联**：[merged-kyber-poly-batch-NTT技术总结.md](merged-kyber-poly-batch-NTT技术总结.md)、[研究路线与frozen治理.md](研究路线与frozen治理.md)

---

## 0. 本文怎么读

| 章节 | 内容 |
|------|------|
| §1 | MIX 三段式 NTT 的数学分段（Split / MMAD / Merge） |
| §2 | 壳层选型：手写 FSM vs 模板 Matmul |
| §3 | 跨核同步与可见性不变量 |
| §4 | poly-batch 与单 TPipe 原则（链到专文） |
| §5 | 路线判决与 frozen 含义 |
| §6 | 案例：探针阶段表与禁止清单 |

**活跃继任**：ML-KEM Tag5T / `fix-f203-2s1e-*` — 见 [MLKEM-NTT-实现总结.md](MLKEM-NTT-实现总结.md)。

---

## 1. 数学：三段式 NTT 在算什么

对模 \(q\) 长度 \(N\) 的多项式，merged_kyber 路线将正向 NTT 拆为：

\[
\text{NTT}(f) = \mathrm{Merge}\bigl(\mathrm{MMAD}(\mathrm{Split}(f), M_0), \mathrm{MMAD}(\mathrm{Split}(f), M_1)\bigr)
\]

| 阶段 | 数学角色 | 典型张量形状（单 poly, \(N=256\)） |
|------|----------|-----------------------------------|
| **Split** | 将 int32 系数拆为 **limb 编码**（本仓 6 bit） | \(f:[N]\) → 两行 int8 hi/lo |
| **MMAD** | 与预计算 LUT 做 **int8 矩阵乘**（两次，\(M_0,M_1\)） | \([2,N]\times[N,*]\to[2,N]\) |
| **Merge** | limb 重组 + **Barrett mod \(q\)** | \([2,N]\to[N]\) int32 |

**与 FIPS Tag5T 的差异**：Merge 语义为 Barrett 路径，golden 对齐 `ntt_sim_kyber`，**不等于** FIPS `MlkemNtt` RouteA。路线关闭后 ML-KEM 以 Tag5T 契约为准。

---

## 2. 壳层选型：为何 FSM + AicMmad，而非 Matmul\<>

### 2.1 不变量

MIX 核内 **AIV（向量）与 AIC（矩阵）交替**；阶段间通过 **GM workspace** 交接，并由 **跨核 flag + barrier** 保证可见性。

| 策略 | 适用 | 风险 |
|------|------|------|
| **手写 FSM** + 显式 `AivSplit` / `AicMmad` / `AivMerge` | 阶段边界清晰、LUT 形状固定 | 需自己维护同步 |
| **高级 Matmul\<>** 融合 encode+matmul | 通用 DL 算子 | NTT 内 **CPU 两段式 ≠ SIM 可交付**；CrossCore 可见性难保证 |

**工程判决（2026-06-11）**：NTT MIX 内 **禁止** 用 `Matmul<>` 替代 `AicMmad`。详见 [NTT-Matmul路线废弃说明.md](NTT-Matmul路线废弃说明.md)。

### 2.2 Stage2 复用原则

Stage2 与单 poly 基线 **同构**：仅左矩阵行数随 batch 变为 \(2k\)（\(k\) 为 poly 数）。**不要**为 batch 换一套 Cube API 族。

---

## 3. 跨核同步不变量

```
AIV Split 写 GM ──SET──► AIC Wait ──MMAD──► 写 GM ──SET──► AIV Wait ──Merge──► 写 dst
```

| 规则 | 原因 |
|------|------|
| AIC 在 `WAIT(split)` **之后**再读 GM | 否则读到未完成 Split |
| 双 AIV **均参与 FSM**；禁止一侧 early-return | 否则抢跑 / 死等 |
| SIM/NPU 在阶段边界 **`PipeBarrier<PIPE_ALL>`** | CPU 顺序执行会掩盖缺失 barrier |
| 固定测试 poly 写入 shared 脚本 | 可复现 golden |

**抽象**：任何「生产者写 GM → 消费者读 GM」的 MIX 链，都需 **显式 happens-before**；不能假设模板或 CPU 顺序替你保证。

---

## 4. poly-batch 与资源生命周期

一次 kernel 处理 \(k\) 条 poly 时：

- **Compute 批量化**：`tileLength = k \cdot (N/2)`，一趟向量核处理所有系数
- **Cube 批量化**：`mRows = 2k`，一次 `AicMmad`
- **禁止**：`for (p) { 构造带 TPipe 的 AivSplit }` — 见 [merged-kyber-poly-batch-NTT技术总结.md](merged-kyber-poly-batch-NTT技术总结.md) §2、[ascendc-TQue与Pipe框架知识库.md](ascendc-TQue与Pipe框架知识库.md) §4.1

Merge 读址：紧凑 \([2k,N]\) 时，poly \(p\) 取行 \(2p,2p+1\)；**不能**照搬单 poly 时「读 4 行、后两行未写≈0」的假设。

---

## 5. 路线判决摘要

| 判决 | 含义 |
|------|------|
| merged_kyber 全线 **frozen** | 数学契约已被 Tag5T / 2s1e 取代 |
| NTT 内 `Matmul<>` **废弃** | 见专文 |
| 6 bit limb | F203 历史参数；7 bit 仅对照 |
| 测试仅在 `ascendc-tests/` | 规格书在 `examples/incubating/` |

**Agent 规则**：读 `FROZEN.md` 知否决原因；**禁止**从 frozen 抄码。见 [研究路线与frozen治理.md](研究路线与frozen治理.md)。

---

## 6. 附录：案例对照

### 6.1 探针阶段（历史）

| 阶段 | 目录（均在 `frozen/`） | 里程碑 |
|------|------------------------|--------|
| D / D′ | `frozen-merged-kyber-ntt256` / `-limb6` | 单 poly CPU+SIM |
| poly2 s123 | `frozen-fix-merged-kyber-…-poly2-s123` | k=2 全链路 batch |
| poly8 s123 | `…-poly8-s123` | k=8 同 poly |
| E | `exp-sepolyvec8-ntt-k8` | 8 互异 poly（后亦冻结） |

执行顺序经验：**D → D′ → k=2 全链 golden → 再扩 k**。

### 6.2 禁止清单 ↔ 原理

| 不要 | 原理章节 |
|------|----------|
| MIX 内 Stage2 用 `Matmul<>` | §2.1 |
| kernel 内 `for (p) new AivSplit` | §4 |
| batch Merge 照搬单 poly 四行读法 | §4 |
| 假设模板处理 CrossCore | §3 |
| 先 batch8 + 大 padding 排错 | §4（先 k=2） |

### 6.3 6 bit 改造（壳内参数）

仅改 limb 位宽与 LUT 切片：`split_2xint6`、`>>6` 移位、golden 仍为 `f @ M mod 3329`。

---

*原 `docs/research/20260610-f203-ntt-mix-merged-kyber开发策略.md`；2026-06-18 迁入 notes 并重构为原理优先。*
