# merged_kyber poly-batch NTT — 技术总结

**读者**：在 AscendC 上实现「单 kernel 多 poly NTT」的开发者  
**目的**：说明 **批量化 = 扩大 tile / 矩阵行数**，而非 host 循环 launch；说明 Merge 读址契约  
**讨论**：[qa/2026-06/2026-06-10-F203-MIX-merged_kyber路线与limb6.md](../../qa/2026-06/2026-06-10-F203-MIX-merged_kyber路线与limb6.md)  
**路线上下文**：[F203-merged-kyber-MIX路线技术总结.md](F203-merged-kyber-MIX路线技术总结.md)

---

## 0. 本文怎么读

| 章节 | 内容 |
|------|------|
| §1 | batch 的数学语义（输入输出族） |
| §2 | 正确批量化模型（单 TPipe、单趟 Compute） |
| §3 | Merge 读址：紧凑矩阵 vs 单 poly 假象 |
| §4 | 故障分类：挂死 vs 算错 |
| §5 | 推广到任意 \(k\) |
| §6 | 案例：k=2/8 探针与验收命令 |

---

## 1. 数学：batch 是什么

对 \(k\) 条长度 \(N\) 的多项式 \(\{f_p\}_{p=0}^{k-1}\)，batch NTT 要求：

\[
\forall p:\quad \mathrm{NTT}(f_p) = \text{与单 poly 基线相同的映射}
\]

**不是**：在 device 上对 \(p\) 循环「多次单 poly 内核语义」而未扩大向量/Cube 批量。

| 张量 | 单 poly | batch（\(k\) poly） |
|------|---------|---------------------|
| src | \([N]\) | \([k,N]\) 行主序 |
| Split 后左矩阵 | \([2,N]\) int8 | \([2k,N]\)，行 \(2p,2p+1\) 为 poly \(p\) 的 hi/lo |
| MMAD 结果 | \(A_0,A_1\) 各 \([2,N]\) | 各 **紧凑** \([2k,N]\) |
| dst | \([N]\) | \([k,N]\) |

---

## 2. 工程模型：单算子实例 + 放大 tile

### 2.1 不变量 P-batch-1：一个 TPipe 生命周期

每个 AIV 向量算子类在 `Init()` 中 **只调用一次** `pipe.InitBuffer`；`CopyIn → Compute → CopyOut` 在同一条 pipe 上完成。

**违反时**：循环内栈上构造 `AivSplit` → 新 `TPipe` → 重复 `AllocEventID` → 挂死（`current id is 8, max … 8`）。

这与 poly 个数 \(k\) **无关** — k=8 失败常因 pipe，而非「batch 太大」。

### 2.2 Stage1：扩大 `tileLength`

- `tileLength = k \cdot (N/2)`
- **Compute**：一趟 `split_vec(local, tileLength)`
- **CopyIn/Out**：`for (p)` **仅** GM 上的 gather/scatter，**不**新建 pipe

### 2.3 Stage2：一次 Cube，左矩阵 \(2k\) 行

- `AicMmad(mRows, n, n)`，`mRows = 2k`
- 全链路须把 M0 写入 workspace 供 Merge 读，而非仅 s12 探针直写 dst

### 2.4 FSM

与单 poly 相同：`SPLIT → SET → AIC WAIT+MMAD → SET → AIV WAIT+MERGE`；保留全管道 barrier。

---

## 3. Merge 读址契约

### 3.1 单 poly 的「隐含零」

单 poly 时 Merge 常读 **4 行** \(i=0..3\)；其中 \(i=2,3\) 对应 **未写的 GM**，读作 ≈0。这在 **紧凑 2 行**布局下成立。

### 3.2 多 poly 紧凑布局

\(A_0,A_1\) 为 \([2k,N]\) 时，\(i=2,3\) 可能是 **另一条 poly 的真实数据**。

**正确规则**（对每个 poly 索引 \(p\)）：

\[
\text{有效行} = 2p\ (\mathrm{hi}),\ 2p+1\ (\mathrm{lo});\quad \text{merge 四槽中另两槽} = 0
\]

实现：`Duplicate` 清零 4 槽，只 `DataCopy` 两行；`ShiftLeft/Add/Barrett` 与单 poly 相同。

**不需要**为 \(k=2\) 做 \(8\times16\) 行 padding。

---

## 4. 故障分类

| 现象 | 类 | 优先查 |
|------|-----|--------|
| `AllocEventID` / 挂死 | **资源** | §2.1 循环内 TPipe |
| max_abs_diff 大、无 assert | **读址** | §3.2 Merge 行号 |
| SIM 慢但无错 | **工程** | trace、dump 路径、WSL 复制目录 |

---

## 5. 推广到 \(k>2\)

1. 仍 **一个 TPipe / 算子实例**；放大 `tileLength`、`mRows=2k`
2. 每个 \(p\)：Merge 取 \(2p,2p+1\)；槽 2/3 恒 0
3. **先 k=2 验证** golden 与取址，再扩 k
4. 注意 workspace `wssize` 与 \(A_0,A_1\) 偏移随 \(k\) 线性增

---

## 6. 附录：案例对照

### 6.1 探针

| k | 目录（`ascendc-tests/frozen/`） |
|---|--------------------------------|
| 2 s12 | `frozen-fix-merged-kyber-ntt256-limb6-poly2-s12` |
| 2 s123 | `…-poly2-s123` |
| 8 s123 | `…-poly8-s123` |
| 8 互异 | `examples/incubating/exp-sepolyvec8-ntt-k8`（历史 exp；交错 S0） |
| 8 紧凑向量 | [`pass-fix-f203-stage123-ntt-intt-polyvec8-vec`](../../ascendc-tests/pass-fix-f203-stage123-ntt-intt-polyvec8-vec/)（**终态**；`[HI₈,LO₈]`；NTT+INTT） |

### 6.2 验收

```bash
cd ascendc-tests/frozen/frozen-fix-merged-kyber-ntt256-limb6-poly2-s123
bash run.sh -r cpu -v Ascend910B4
```

期望：`max_abs_diff=0`；`dst` 每行 ≡ 单 poly limb6 golden。

### 6.3 数据布局（k=2）

```text
src [2,256]  行主序
S0  [4,256]  row0=hi0, row1=lo0, row2=hi1, row3=lo1
A0,A1 [4,256]
dst [2,256]
```

### 6.4 踩坑 ↔ 原理

| 误判 | 实际根因 | § |
|------|----------|---|
| batch8 太大 | 循环新建 TPipe | §2.1 |
| 要 pad 成 8×16 | Merge 读址错误 | §3 |
| 多次单 poly 调用 | tileLength 未放大 | §2.2 |

---

*原 `docs/research/20260610-merged-kyber-6bit-双poly批量化NTT调通纪要.md`；2026-06-18 迁入 notes 并重构。*
