# FIPS 203 Compress / Decompress — 统一整数舍入技术总结

**读者**：未参与本仓库开发的实现者 / Agent  
**目的**：说明 **Compress_d** 如何用「`2^T/q` 近似整数 + 幂次 `y` 消分母」化为**纯乘加移位**；为何利于 **常数时间** 与 **AscendC 全 d 向量**；**Decompress** 的配套整数式  
**案例锚点**：本仓现行探针 [`pass-f203-compress-d-vec-k4`](../../ascendc-tests/pass-f203-compress-d-vec-k4/) · [`pass-f203-decompress-d-vec-k4`](../../ascendc-tests/pass-f203-decompress-d-vec-k4/)（§6）；Encrypt tail 见 [`F203-Compress-Decompress-向量实现指南.md`](F203-Compress-Decompress-向量实现指南.md)  
**讨论**：[`qa/2026-07/2026-07-10-Decrypt交付stable.md`](../../qa/2026-07/2026-07-10-Decrypt交付stable.md) §14（2026-07-10 定稿思路）

---

## 0. 本文怎么读

| 章节 | 内容 | 是否依赖本仓库代码名 |
|------|------|----------------------|
| §1 | FIPS 舍入与「难算」根源 | 否 |
| §2 | **统一 Compress 推导**（`y` 消分母、`2^37/q`） | 否 |
| §3 | Decompress 整数式 | 否 |
| §4 | **常数时间**与 DJB 类关切 | 否 |
| §5 | **AscendC 向量化**与对本仓路线的关系 | 少量 |
| §6 | 常数表、验收、案例对照 | 是 |

---

## 1. 数学与数据契约

FIPS 203 §4.2.1（`q = 3329`，`u ∈ [0,q)`，`c ∈ [0,2^d)`）：

```text
Compress_d(u)   = round(u · 2^d / q) mod 2^d
Decompress_d(c) = round(c · q / 2^d)
```

**难点（原理层）**：`q` 为素数，`2^d` 不能被 `q` 整除，`u·2^d/q` 在实数上「几乎总是非整数」；直接写除法在整数 CPU/NPU 上既慢，又在许多架构上**不是常数时间**。DJB 等对 Kyber/ML-KEM **Compress** 的批评，针对的正是**变延迟除法 / 依赖实现细节的舍入路径**。

**目标**：全程 **整数 + 固定指令序列**（乘、加常数、算术右移），且 **per-lane 可向量化**。

---

## 2. 工程不变量 — 统一 Compress（`y` 消分母）

### 2.1 从 `round` 到 `floor`

```text
round(a) = ⌊ a + 1/2 ⌋
```

对 `a = u·2^d/q`：

```text
round(u · 2^d / q) = ⌊ u · 2^d / q + 1/2 ⌋
```

「`+1/2`」在整数实现里不是浮点 0.5，而是后续与 **`y/2`** 合并的舍入项。

### 2.2 同乘 `y` 清分母

对任意正整数 `y`：

```text
u · 2^d / q + 1/2 = (u · y · 2^d / q + y/2) / y
```

故：

```text
round(u · 2^d / q) = ⌊ (u · y · 2^d / q + y/2) / y ⌋
```

**约束 1**：取 **`y = 2^k`** → 除以 `y` 变为 **`>> k`**（AscendC `ShiftRight`）。

**约束 2**：令 **`y · 2^d / q` 极接近整数**，记：

```text
C = ⌊ y · 2^d / q ⌋ ≈ y · 2^d / q     （小数部分可忽略）
```

则实现形态为：

```text
Compress_d(u) ≈ ⌊ (C · u + y/2) / y ⌋ = (C · u + 2^(k-1)) >> k
```

**全程**：一次整数乘、一次加常数、一次算术右移；**无 `/q`、无浮点、无按 `u` 分支**。

### 2.3 为何选 `2^37 / q` 与统一常数 `C`

寻找指数 **`T`**，使 **`2^T / q` 几乎为整数**：

```text
2^37 / 3329 = 41285357.005707…
2^37 = 41285357 × 3329 + 19        // 余数仅 19，相对 2^37 可忽略
```

取 **`C = ⌊2^37 / q⌋ = 41285357`**，作为**所有档位共用的乘数**（与 `d` 无关）。

对给定 **`d`**，由 **`C ≈ y·2^d/q`** 与 **`C = 2^37/q`** 得：

```text
y · 2^d ≈ 2^37   ⟹   y = 2^(37 - d) ，k = 37 - d
```

**通式（ML-KEM 各档 `d`）**：

```text
round(u · 2^d / q) = ⌊ (C · u + 2^(36-d)) / 2^(37-d) ⌋
                   = (C · u + (1 << (36-d))) >> (37-d)
```

| d | 用途（1024） | y = 2^(37-d) | bias `y/2` | 右移 |
|---|--------------|--------------|------------|------|
| 11 | c₁ / u | 2^26 | 2^25 | >> 26 |
| 10 | u（768） | 2^27 | 2^26 | >> 27 |
| 5 | c₂ / v | 2^32 | 2^31 | >> 32 |
| 4 | v（768） | 2^33 | 2^32 | >> 33 |
| 1 | Compress₁ | 2^36 | 2^35 | >> 36 |

**误差为何可忽略（直观）**：`C` 相对真值 `y·2^d/q` 的偏差来自 **`2^37 mod q = 19`** 量级；最后除以 **`y = 2^(37-d)`**，且 **`y ≫ 2^d`**，该偏差在整数舍入边界内不足以改变 **`⌊·+y/2⌋`** 的结果（对 **`u ∈ [0,q)`** 已用脚本穷举验证，见 §6）。

### 2.4 可复用模式 P-UCOMP（Unified Compress）

1. 选定 **`T`** 使 **`{2^T/q}`** 足够小 → **`C = ⌊2^T/q⌋`**。  
2. 对每档 **`d`**：**`k = T - d`**，**`bias = 2^(k-1)`**，实现 **`(C·u + bias) >> k`**。  
3. 输入 **`u`** 须 **canonical mod q**（`[0,q-1]`）；输出必要时 **`& ((1<<d)-1)`**（当移位后理论上可达 `2^d` 时；对 `d=11` 等档移位结果已在范围内则可省略）。  
4. 中间量 **`C·u + bias`**：最大约 **`C·(q-1) + 2^(T-d-1)`**；**`T=37`** 时 **`≪ 2^63`**，**`int64` 安全**。

---

## 3. Decompress — 对称的整数舍入

```text
round(c · q / 2^d) = ⌊ (c · q + 2^(d-1)) / 2^d ⌋ = (q · c + (1 << (d-1))) >> d
```

**无需** 再选 **`2^T/q`**：`q` 为整数，分母 **`2^d`** 为幂次。  
与本仓 [`decompress_d_vec.hpp`](../../ascendc-tests/pass-f203-decompress-d-vec-k4/decompress_d_vec.hpp) **同式**（向量：`Muls(q) → Adds(2^(d-1)) → ShiftRight(d)`）。

**与浮点 `round` 的 5 个 tie 点**（`y·q/2^d` 恰为半整数）：整数式给出 **`⌊(y·q+2^(d-1))/2^d⌋`**，与 FIPS / liboqs / 本仓 golden **一致**；不以 Python `round()` 为准。

---

## 4. 常数时间（CT）论证要点

| 关切 | 本路线 | 本仓现行部分路径 |
|------|--------|------------------|
| 整数 `/` 或 `% q` | **无** | 无（Barrett 亦无） |
| 浮点 `Div` | **无** | d=10/11 Compress 用 **float `Div`**（[`compress_d_vec.hpp`](../../ascendc-tests/pass-f203-compress-d-vec-k4/compress_d_vec.hpp) P-COMP-2） |
| 按秘密 `u` 分支 | **无**（固定 `k`、`C`、`bias`） | 无 |
| 查表 / Gather | **无** | 无 |
| 指令序列 | 每 lane **`Muls` → `Adds` → `ShiftRight`** | d=4/5 同；d=10/11 多 **Cast/Div** |

在 **AscendC 向量 ISA** 上，上述三步对 **256 系数** 为 **固定深度** 的 per-lane 运算；**`ShiftRight` 立即数** 为数据无关移位。  
**前提**：`u`/`c` 已 canonical；**`d` 为公开参数**（编译期常数）；不在 Compress 热路径做 secret-dependent 约化。

**结论（工程口径）**：该推导天然避开 DJB 所指「**用原生除法做 Compress**」；比本仓 **d=11 cast_div** 更利于 CT 叙述。  
**未声明**：NPU 微架构侧信道（功耗、争用）需实机/模型单独评估；本文仅 **算法 + 指令形态** 层。

---

## 5. AscendC 向量化落地

### 5.1 统一 Compress 向量核（推荐形态）

对单 poly **`N=256`**，`LocalTensor<int32_t>`：

```text
Muls(tmp_or_out, in, C, N)           // C = 41285357
Adds(tmp_or_out, tmp_or_out, bias, N) // bias = 2^(36-d)
ShiftRight(out, tmp_or_out, k, N)     // k = 37-d
// 若 d 需显式 mod 2^d：mask_low_bits（移位/乘/减，见现有指南）
```

- **所有 `d ∈ {1,4,5,10,11}` 同一结构**，仅编译期 **`bias`、`k`** 不同 → **可宏展开，无需 float UB**。  
- 相对本仓指南 **P-COMP-2**（d=10/11）：**省 `3×256×float` scratch**，SIM tick 预期更优，且与 CT 一致。

### 5.2 Decompress 向量核（已验收）

```text
Muls(tmp, in, q, N) → Adds(tmp, 2^(d-1)) → ShiftRight(d)
```

见 [`F203-Compress-Decompress-向量实现指南.md`](F203-Compress-Decompress-向量实现指南.md) §2 P-DEC。

### 5.3 与本仓现行实现的关系

| 项 | 本仓现行 | 统一整数舍入（本文） |
|----|----------|----------------------|
| d=4/5 Compress | 分档 Barrett magic | **同一 `C`，不同 `k`** |
| d=10/11 Compress | cast_div + float | **同一 `C`，int32 向量 limb 宽乘** |
| 正确性 | 已 PASS | 与 FIPS round **全域 0 diff**（`u∈[0,q)`，脚本） |
| 设备探针 | `pass-f203-compress-d-vec-k4` | **`pass-f203-compress-unified-int-vec-k4`**（2026-07-10） |
| Decompress 探针 | `pass-f203-decompress-d-vec-k4` | **`pass-f203-decompress-unified-int-vec-k4`**（含 d=1） |

**迁移门禁**：`pass-f203-compress-d-vec-k4` CPU+SIM + `liboqs_pke_vs_ascendc.sh` Encrypt **c** 分段；再改 stable tail。

---

## 6. 案例附录

### 6.1 参考 C++ 形态（原理，非本仓源码）

```cpp
constexpr int64_t C = 41285357;  // floor(2^37 / 3329)
constexpr int32_t Q = 3329;

// d=11 例：k=26, bias=2^25
inline int32_t compress_d11(int32_t x) {
    return static_cast<int32_t>((C * x + (1LL << 25)) >> 26);
}

inline int32_t decompress_d(int32_t y, int d) {
    return (Q * y + (1 << (d - 1))) >> d;
}
```

### 6.2 验收脚本（原理验证）

对 **`C=41285357`**、**`T=37`**，五档 **`d`**，**`u=0..3328`** 与 **`round(u·2^d/q) mod 2^d`** 穷举 **0 mismatch**；**`T=34` 失败、`T≥35` 对统一 `C` 成立**（最小 **`T=35`**，选用 **`37`** 为余量更大的「近似整数」点）。

### 6.3 本仓文件索引

| 路径 | 角色 |
|------|------|
| `ascendc-tests/pass-f203-compress-d-vec-k4/compress_d_vec.hpp` | 现行设备 Compress（分档 Barrett + cast_div） |
| `ascendc-tests/pass-f203-compress-unified-int-vec-k4/` | **统一整数** Compress（int32 向量 limb；C0=63213,C1=629） |
| `ascendc-tests/pass-f203-decompress-d-vec-k4/decompress_d_vec.hpp` | 设备 Decompress（已与 §3 一致） |
| `ascendc-tests/pass-f203-decompress-unified-int-vec-k4/` | **统一整数** Decompress（int32 全向量；d=1/4/5/10/11） |
| `docs/notes/F203-PKE-liboqs交叉验证与Compress定点技术总结.md` | d=5 bias 历史 bug、L2 oracle |
| `docs/notes/F203-Compress-Decompress-向量实现指南.md` | 抄码清单、tail 关系 |

---

## 7. 可复用模式目录

| ID | 模式 | 一句话 |
|----|------|--------|
| **P-UCOMP-1** | `round(u·2^d/q)` → `(C·u + 2^(k-1))>>k` | 选 `C=⌊2^T/q⌋` 近整数，`k=T-d` |
| **P-UCOMP-2** | 统一 `C` 多档 `d` | `y=2^(T-d)` 由同一 `T` 倒推 |
| **P-UDEC-1** | `round(c·q/2^d)` | `(q·c + 2^(d-1))>>d`，全 d 同形向量 |
| **P-CT-1** | Compress CT 友好 | 禁 `/`、禁 secret 分支、禁 float 商 |
| **P-VEC-1** | AscendC | `Muls/Adds/ShiftRight` 256-wide，无 float UB |
