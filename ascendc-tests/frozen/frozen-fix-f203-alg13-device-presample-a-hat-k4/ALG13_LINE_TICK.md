# Alg.13 行号 — AscendC SIM tick 对照（ML-KEM-1024，$k{=}4$）

**环境**：Ascend910B4 CaModel，`SEED_D=20260619`  
**行号**：FIPS 203 **Algorithm 13**（K-PKE.KeyGen）原文步骤  
**说明**：当前工程 **未** 在单 kernel 内跑完全文 KeyGen；下表按 **实际落地探针** 归类 tick。

---

## 1. 总览（按 Alg.13 行段）

| Alg.13 行 | 密码语义 | 当前执行位置 | AscendC SIM tick | 测法 |
|-----------|----------|--------------|------------------|------|
| **1–2** | $(\rho,\sigma)\leftarrow G(d\|k)$；$N\leftarrow 0$ | 设备 **部分**（见 §2） | **~3–8k**（粗估） | 未单独门控；含于 8–15 的 Phase G |
| **3–7** | $\widehat{A}[i,j]\leftarrow \mathrm{SampleNTT}(\rho\|j\|i)$ | 设备 [`a-hat-k4`](.) Phase A | **~586k 增量**（全段 **719237** 标量 / **881627** shake_vec+scalar rej） | `SE_A_HAT_STAGE` / `SE_A_HAT_REJ` 见 [`SIM_BENCHMARK.md`](SIM_BENCHMARK.md) |
| **8–15** | $s_i,e_i$ ← PRF+CBD | 设备 [预采样 V3](.) | **133153** | V3 门控直接测 |
| **16–17** | NTT($\mathbf{s}$), NTT($\mathbf{e}$) | 设备链式 [`CHAIN_NTT17`](CHAIN_NTT17.md) 或 v2 `mixPass=5` | **44400** / **44648** | 链式 Device `src` / v2 Host tiled `src` |
| **16–20** | NTT、$\widehat{t}$、ByteEncode | 设备 [`vec-k4-v2`](../pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/) | **77958** | v2 全链路（**不含** 8–15） |

**分 launch 设备合计（行 1–2 份额 + 8–20，不含 3–7）**：粗估 **~211k**（133153 + 77958，G 不重复计）。

**链式探针（行 8–17，已验收）**：**~177553**（133153 + 44400）；见 §5.1。

---

## 2. 行 1–2 详解

| 规范步骤 | 本工程实现 | 设备？ | tick |
|----------|------------|--------|------|
| 输入 $d$ | `DerandFromSeedD(SEED_D)` → SHA3-256(derand 字符串) | 是（标量） | 含于 Phase G |
| 行 1：$G(d\|k)\to(\rho,\sigma)$ | `HashGSigma` → SHA3-512($d\|k$)，**仅用 $\sigma$**[32:64]；$\rho$ 不落盘 | 是（标量） | 含于 Phase G |
| 行 2：$N\leftarrow 0$ | PRF 循环 $N{=}0..7$ 隐式满足 | — | **0**（无额外指令） |

**要点**：

- 行 1–2 **没有** 独立 AscendC 探针；与行 8–15 合在 **同一 launch**（`f203_se_vector_k4`，`blockDim=1`）。
- $\rho$ 在行 3–7 才需要，但 **3–7 在 Host** 完成，故设备侧不算 $\rho$ 的后续使用开销。
- Phase G tick **未实测**，按 2× Keccak-f[1600] 量级粗估 **~3–8k**（见 [`SIM_BENCHMARK.md`](SIM_BENCHMARK.md) §2）。

---

## 3. 行 3–7 详解

```text
3: for i = 0 .. k-1
4:   for j = 0 .. k-1
5:     A[i,j] ← SampleNTT(ρ || j || i)
6:   end
7: end
```

| 项 | 值 |
|----|-----|
| **设备 AscendC tick** | **0** |
| **数据** | `vec-k4-v2` 读 Host 写入的 `input/a_hat.bin`（`int32 [16,256]`） |
| **原因** | 预采样段 [本探针](.) 与 2s1e 集成规划 **不包含** $\widehat{A}$ 矩阵生成 |

将来若下放 3–7，需单独探针 + 与现有 `a_hat` 布局对齐；**与当前 V3 集成无关**。

---

## 4. 行 8–15 详解（设备预采样 V3）

### 4.1 规范 ↔ 工程

| Alg.13 行 | 语义 | `src` 行 |
|-----------|------|----------|
| 8–11 | $s_i \leftarrow \mathrm{CBD}(\mathrm{PRF}(\sigma,N))$，$N{=}0..3$ | 0–3 |
| 12–15 | $e_i \leftarrow \mathrm{CBD}(\mathrm{PRF}(\sigma,N))$，$N{=}4..7$ | 4–7 |

### 4.2 全段 tick（V3 锁定）

| 度量 | tick |
|------|------|
| **行 8–15 全段** | **133153** |

### 4.3 段内拆分（实现类型 + 差分/粗估）

| 子段 | Alg.13 | 实现 | tick | 测法 |
|------|--------|------|------|------|
| **G** | 行 1 的 $\sigma$ 部分（+ 工程 derand $d$） | **标量** Keccak | **~3–8k** | 粗估 |
| **P** | 行 8–15 的 8× PRF | **向量** shake batch | **83478**（含 G） | V1 门控 |
| **C** | 行 8–15 的 8× CBD | **向量** P1b-single | **49675** | V3−V1 差分 |

### 4.4 行 8–11 vs 12–15（粗估，非门控）

PRF 为 **8 路 batch 一次做完**，无法在不改 kernel 的情况下单独测 $N{=}0..3$ 与 $4..7$。CBD 为 **8 行对称**，可按 tick **对半** 粗估：

| 行段 | 内容 | 粗估 tick | 说明 |
|------|------|-----------|------|
| **8–11** | $\mathbf{s}$：4×(PRF+CBD) | **~64000–67000** | $\frac{1}{2}$P + $\frac{1}{2}$C + 分摊 G |
| **12–15** | $\mathbf{e}$：4×(PRF+CBD) | **~64000–67000** | 同上 |
| **合计** | | **133153** | |

更细的拆法（在 $G{\approx}5\mathrm{k}$ 假设下）：

| 份额 | 8–11 ($\mathbf{s}$) | 12–15 ($\mathbf{e}$) |
|------|---------------------|----------------------|
| PRF（估） | ~39k | ~39k |
| CBD（估） | ~25k | ~25k |

---

## 5. 行 16–20（集成上下文）

### 5.1 链式探针（行 8–17，Device `src`）— ✅ PASS

[`CHAIN_NTT17.md`](CHAIN_NTT17.md)：`bash run_chain_ntt17.sh -r cpu|sim -v Ascend910B4`

| Launch | 行段 | SIM tick | 测法 |
|--------|------|----------|------|
| Launch1 | 8–15（V3） | **133153** | 同 V3 standalone |
| Launch2 | 16–17（mixPass=5） | **44400** | Device 真实 CBD `src`；定标 44648 |
| **合计** | 8–17 | **~177553** | 分阶段 SIM；tick ≈ 两段之和 |

结论：**无链式异常** — 真实 4×不同 $s_i$ 不改变 NTT tick 量级；对拍 `max_abs_diff=0`。

### 5.2 vec-k4-v2 分段（Host `src`，非设备 8–15）

[`vec-k4-v2`](../pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/SIM_BENCHMARK.md) 分段：

| 行段 | 内容 | SIM tick |
|------|------|----------|
| 16–17 | NTT($\mathbf{s}$), NTT($\mathbf{e}$) | **44648**（mixPass=5） |
| 18 | $\widehat{\mathbf{t}}\leftarrow\widehat{\mathbf{A}}\circ\widehat{\mathbf{s}}+\widehat{\mathbf{e}}$ | **+20840**（相对 16–17） |
| 19–20 | ByteEncode₁₂ → ek/dk | **+12421**（prefetch） |
| **16–20 合计** | | **77958** |

---

## 6. 一张图（当前集成规划）

```text
行 1–2   Host+Device 标量 G(σ)     ~5k      ─┐
行 3–7   Host SampleNTT → a_hat      0       │  KeyGen 预采样 / 2s1e 输入
行 8–15  Device 预采样 V3           133153     ─┘  launch 1, blockDim=1
行 16–17 Device chain / mixPass=5   44400        launch 2 前半（链式已验 ✅）
行 18–20 Device vec-k4-v2 余段     +33310       launch 2 后半（18 dot + 19–20）
────────────────────────────────────────
链式至 17（已测）                 ~177553
设备 tick 合计 8–20（不含 3–7）   ~211k
```

---

## 7. 复现

```bash
# 行 8–15（V3）
cd ascendc-tests/pass-fix-f203-alg13-lines8-15-se-k4
SE_VECTOR_STAGE=v3 bash run.sh -r sim -v Ascend910B4

# 行 8–17 链式（Device src → MIX NTT）
bash run_chain_ntt17.sh -r sim -v Ascend910B4

# 行 16–20
cd ascendc-tests/pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2
bash run.sh -r sim -v Ascend910B4
```

**待补**：G-only 门控探针，将行 1–2 / Phase G 从粗估改为实测。
