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
| §7 | 可复用模式目录 | 否 |
| §8 | **相关工作与业界对比**（优缺点、选型） | 否 |

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

---

## 8. 相关工作与业界对比

**读者**：选型、对外说明、评审「相对 mlkem-native / OpenSSL 有何不同」时阅读。  
**结论先行**：本路线属于 ML-KEM **Compress 乘加移位定点**成熟技术族；**Decompress 与业界标准式相同，差异化几乎全在 Compress**。相对分档 magic / Barrett / float 等，本仓 **P-UCOMP（T=37）** 的取舍是：**用略宽的中间精度与更长的 per-d 移位，换全 d 同构、CT 叙述统一、AscendC 向量路径不分叉**。

### 8.1 对比对象（Compress 侧）

| 代号 | 代表实现 | 核心形态 |
|------|----------|----------|
| **A. 分档 magic** | [mlkem-native `compress.h`](https://github.com/pq-code-package/mlkem-native/blob/main/mlkem/compress.h)、本仓 P-COMP-1 | 每档 `d` 独立 `(M_d, bias, shift)`，如 d=5：`1290176`、`+2^26`、`>>27` |
| **B. Barrett 商+余** | [OpenSSL / BoringSSL ML-KEM](https://github.com/openssl/openssl/blob/master/crypto/ml_kem/ml_kem.c) | `(u<<d)*μ>>s` 得商，余数 CT 调舍入 |
| **C. 模板统一 m** | [Botan `kyber_helpers.h`](https://botan.randombit.net/doxygen/kyber__helpers_8h_source.html) | 统一 `m=2580335, p=33`，输入先 `(x<<d)+q/2` |
| **D. 硬件统一 T=35** | [US 11,632,242](https://patents.justia.com/patent/11632242)（PQSecure, 2023） | `q′=⌊2^35/q⌋`，`>> (35-d)` + 看特定位舍入 |
| **E. float 商** | 本仓旧 P-COMP-2 | d=10/11：`Cast→Div(3329)→截断` |
| **F. 参考除法** | pq-crystals ref | `((u<<d)+q/2)/q`（[KyberSlash](https://eprint.iacr.org/2024/1049.pdf) 风险源） |

**Decompress**：各路线普遍为 **`(q·c + 2^(d-1)) >> d`**（见 §3），与 A–F 无实质分歧。

### 8.2 与近似原理路线的关系

**同族（非从零发明）**：

- **D**：「单一乘数 + 按 d 变移位」与 P-UCOMP 同构；差异在 **T 选取**（专利 T=35 vs 本仓 T=37）与 **舍入写法**（看 bit vs 显式 `bias=2^(k-1)`）。
- **A / B / C / E**：均属「用乘法近似 `/q` 或 `/2^d`」，Hacker's Delight 式常数除法；[Filippo mlkem768](https://words.filippo.io/mlkem768/) 亦强调 Compress 宜 Barrett、忌原生 `/`。

**本仓特有（公开文献未检到相同闭式）**：

- **`C=41285357=⌊2^37/q⌋`** 作**全 d 共用**乘数，**`k=37-d`、`bias=2^(36-d)`** 派生；
- 以 **`2^37 mod q = 19`** 与 **`u∈[0,q)` 穷举**（§6.2）作为选 T 门禁。

**不宜对外表述**：「首个提出 Compress 整数实现 / 首个统一全 d 的 Compress」——硬件专利与后续 FPGA 论文已有「统一常数、按 d 变移位」类叙述。

### 8.3 优点（相对业界分档 / float / 参考除法）

| 维度 | 说明 |
|------|------|
| **全 d 同构** | 单一 `C`；AscendC 五档同一 `Muls→Adds→ShiftRight` 骨架，不必 d≤5 用 int32 Barrett、d≥10 另开 float（对比 [`向量实现指南`](F203-Compress-Decompress-向量实现指南.md) P-COMP-1/2 分裂） |
| **CT 叙述** | 无 `/`、无 float `Div`、无按秘密 `u` 分支；较 B 指令更浅，较 F/E 可审计性更好 |
| **维护 / 抄码** | 常数从「每 d 三元组」收成「一个 C + 派生 k/bias」；tail / stable / 探针共用 [`library/shared/f203_unified_round/`](../../library/shared/f203_unified_round/) |
| **精度门禁清晰** | T 有闭式余数界 + 穷举；相对 D 的 T=35，T=37 余量更大 |
| **Decompress 对称** | Compress 用 `2^T/q` 消分母；Decompress 用 `2^d` 消分母——文档可成对叙述 |

### 8.4 缺点与代价

| 维度 | 说明 |
|------|------|
| **非 per-d 最优** | A 为每 d 单独调 `(M_d,K)`，该档近似最紧；P-UCOMP 用全局 C 折中，**正确性依赖「T 够大」**（T=34 失败，§6.2） |
| **移位可能更长** | d 小则 `k=T-d` 大（d=4 时 `>>33`）；A/C 常可缩短 K；**裸 CPU 周期未必最优** |
| **中间乘积更宽** | `C·u` 需 64 位或 limb 宽乘；A 的 d=4/5 可全程 u32 lane |
| **Decompress 无增量** | 与 mlkem-native 等相同，**不构成差异化** |
| **参数变更** | 若 `q` 或 d 集合变，需**重找 T**；A 可逐档重算 magic |
| **前置条件** | `u` 须 canonical `[0,q-1]`；`d` 须编译期公开 |

### 8.5 分维度对照（Compress）

| 维度 | P-UCOMP (T=37) | 分档 magic (A) | Barrett 商+余 (B) | Botan (C) | float (E) |
|------|----------------|----------------|-------------------|-----------|-----------|
| 全 d 代码同构 | ★★★★★ | ★★ | ★★★ | ★★★★ | ★★ |
| CT 可论证性 | ★★★★★ | ★★★★★ | ★★★★ | ★★★★★ | ★★★ |
| 每 d 精度余量 | ★★★★ | ★★★★★ | ★★★★★ | ★★★★ | ★★★★★ |
| 中间位宽 | 64b / limb | d=4/5 可 u32 | 视实现 | 中等 | float 有效位 |
| CPU 裸周期 | 可能略逊 A | 常较优 | 指令较多 | 较好 | d=10/11 或最快 |
| AscendC/NPU 向量 | ★★★★★ | ★★★★ | ★★★ | ★★★★ | ★★ |
| 维护成本 | ★★★★★ | ★★★ | ★★★ | ★★★★ | ★★ |

### 8.6 选型建议（本仓语境）

**优先 P-UCOMP**：

- AscendC 向量：**一种 kernel 覆盖 d∈{1,4,5,10,11}**；
- 交付要求全链路 CT、禁 float Div；
- Encrypt tail / stable 与探针共用统一头文件。

**仍可保留分档 magic / float 的场景**：

- 极致 **CPU** 单档性能（尤其 d=10/11 旧 cast_div 在本仓曾有较低 SIM tick，见向量指南 §2 P-COMP-2）；
- 16 位 / 无 64 位嵌入式；
- 与 **liboqs / mlkem-native 字节级同源**，减少第三方审计差异。

**推荐对外表述**：

> 本方案属于 ML-KEM Compress 的常数时间整数定点实现；与 US 11,632,242 等同属「2^T/q 近似 + 统一乘数、按 d 变移位」族，选用 **T=37** 与显式舍入 bias，并在 `u∈[0,q)` 上穷举验证；Decompress 采用 FIPS 203 标准整数式。

### 8.7 外部参考（附录）

| 类型 | 链接 / 标识 |
|------|-------------|
| 标准 | [FIPS 203](https://nvlpubs.nist.gov/nistpubs/fips/nist.fips.203.pdf) §4.2.1 |
| 侧信道 | [KyberSlash (ePrint 2024/1049)](https://eprint.iacr.org/2024/1049.pdf) |
| 软件 CT 实践 | [mlkem768 / Filippo](https://words.filippo.io/mlkem768/) · [mlkem-native compress.h](https://github.com/pq-code-package/mlkem-native/blob/main/mlkem/compress.h) |
| 硬件统一常数 | [US 11,632,242](https://patents.justia.com/patent/11632242) · [PMC unified Kyber+Dilithium (2025)](https://pmc.ncbi.nlm.nih.gov/articles/PMC12190417/)（移位-加法链形态，非同一 C） |
| 本仓对照 | [`F203-PKE-liboqs交叉验证与Compress定点技术总结.md`](F203-PKE-liboqs交叉验证与Compress定点技术总结.md) · [`F203-Compress-Decompress-向量实现指南.md`](F203-Compress-Decompress-向量实现指南.md) |
