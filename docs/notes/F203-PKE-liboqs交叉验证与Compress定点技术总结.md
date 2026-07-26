# FIPS 203 PKE — liboqs 交叉验证与 Compress 定点技术总结

**读者**：未参与本仓库开发的实现者 / Agent  
**目的**：说明 PKE 三层验收分工、如何用 liboqs 做分阶段字节 oracle，以及 **`Compress_d` 定点舍入偏置** 的可复用契约  
**案例锚点**：[`pass-f203-compress-d-vec-k4`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-f203-compress-d-vec-k4/)（**d=4/5/10/11**）；向量指南 [`F203-Compress-Decompress-向量实现指南.md`](F203-Compress-Decompress-向量实现指南.md)  
**讨论**：[`qa/2026-07/2026-07-01-liboqs验证与KEM-Alg19-KeyGen规划.md`](../../qa/2026-07/2026-07-01-liboqs验证与KEM-Alg19-KeyGen规划.md)  
**实现方案**：Encrypt [`INTEGRATION_PLAN.md`](../../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-encrypt-k4/INTEGRATION_PLAN.md) §10

---

## 0. 本文怎么读

| 章节 | 内容 | 是否依赖本仓库代码名 |
|------|------|----------------------|
| §1 | ml_kem_1024 密文分段与 Compress 数学 | 否 |
| §2 | 定点 `Compress_d` 舍入偏置契约 | 否 |
| §3 | PKE 验收分层（golden / liboqs / round-trip） | 否 |
| §4 | 分段定位方法论 | 否 |
| §5 | 案例与文件映射 | 是 |

---

## 1. 数学与数据契约

ml_kem_1024 密文（1568 B）：

```text
c = c₁ ‖ c₂
c₁ = 4 × ByteEncode₁₁( Compress₁₁(u[j]) )   // 4×352 = 1408 B
c₂ = ByteEncode₅( Compress₅(v) )             // 160 B
```

FIPS 203 Eq. (4.7)（canonical \(u \in [0,q)\)，\(q=3329\)）：

```text
Compress_d(u) = round( u · 2^d / q ) mod 2^d
```

实现常用 magic 常数 \(M_d \approx 2^d \cdot 2^K / q\)（\(K\) 为中间精度），再：

```text
round( u · M_d / 2^K )  ≈  ( u·M_d + bias ) >> K
```

**偏置 `bias` 必须与 \(K\) 匹配**：对 `>> K` 实现 `round`，取 **`bias = 2^{K-1}`**，不是 `2^K`。

---

## 2. 工程不变量 — Compress_5 / Compress_11

### 2.1 ml_kem_1024 常用参数（与 liboqs mlkem-native ref 一致）

| d | magic \(M_d\) | 移位 K | **正确 bias** | 掩码 |
|---|---------------|--------|---------------|------|
| **5** | `1290176` (= `2^5 · round(2^27/q)`) | 27 | **`1 << 26`** | `& 0x1F` |
| **11** | `5284526080` (= `2^11 · round(2^33/q)`) | 33 | **`1 << 32`** | `& 0x7FF` |

**错误模式（本仓 2026-07-01 前）**：d=5 误写 `(d0 + (1<<27)) >> 27`，等价于 bias 过大一档，约 **45%** 系数在边界附近与 liboqs 差 1。d=11 正确，故 **c₁ 可完全对齐而 c₂ 全错**。

### 2.2 参考实现（C / Python 标量）

```c
// Compress_5 — 对齐 liboqs mlk_scalar_compress_d5
uint32_t d0 = (uint32_t)u * 1290176u;
return (d0 + (1u << 26)) >> 27;   // 再 & 0x1f 若需显式截断
```

```python
# host golden 同式
return ((u * 1290176 + (1 << 26)) >> 27) & 0x1F
```

**Decompress_5** 不受此 bug 影响（`((u * q) + 16) >> 5`），Encrypt 侧重 pack 路径。

### 2.3 可复用模式 P-C1

新增或 vendored **`Compress_d`** 时：

1. 以 **liboqs / PQClean / FIPS 伪代码** 为 oracle，不只与仓库内 golden 自洽。
2. 核对 **(magic, K, bias)** 三元组；d=5 与 d=11 **不可类推 bias 指数**。
3. 密文 FAIL 时 **先按 c₁/c₂ 切段**，再查 pack 前多项式系数是否已一致。

---

## 3. PKE 验收分层

| 层 | 入口 | 证明什么 | 不证明什么 |
|----|------|----------|------------|
| **L1** 探针 `run.sh` | `ENCRYPT_VERIFY=1` 等 | device I/O vs **host golden** | 与 liboqs 一致 |
| **L2** liboqs 三阶段 | `liboqs_pke_vs_ascendc.sh` | KeyGen / Encrypt / Decrypt 各 vs **liboqs 字节** | 跨算子 device 串联 |
| **L3** round-trip | `roundtrip_pke_encrypt_decrypt.sh` | device **c → m** 闭环 | 外部 oracle |

三层 **互补**；探针 [`SELF_CONTAINED.md`](../../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-encrypt-k4/SELF_CONTAINED.md) 禁止 liboqs 渗入默认 `run.sh`，L2 放在 **仓库根 scripts/**。

**L2 输入约定**（固定种子可复现）：

- `SEED_D` → KeyGen `d`、Encrypt `m/coins`（`RNG(SEED_D+991)` 与探针 `gen_data` 一致）。
- Encrypt 阶段：AscendC **ek** + liboqs **m/coins** → 对拍 **c**。
- Decrypt 阶段：AscendC **dk + c** → 对拍 **m**（及 liboqs 侧 `m_rec`）。

---

## 4. 验证方法论 — 分段定位

当 L2 Encrypt **c** FAIL 且 L1 PASS：

```text
1. 比较 c[0:1408] 与 c[1408:1568] 各自 max diff
2. 若仅 c₂ 错 → 查 Compress_5 / ByteEncode_5 / v 链 pack 前系数
3. 若 c₁ 错 → 查 u / Compress_11 / NTT·Â 链
4. pack 前系数已一致 → 几乎一定是 Compress/Encode 标量语义
5. pack 前不一致 → 才查 NTT、CBD、embed_message 等
```

本案例：**c₁ max=0，c₂ 全 diff，v 系数 pack 前 max=0** → 直接锁定 `Compress_5` bias。

---

## 5. 案例附录

| 项 | 值 |
|----|-----|
| SEED_D | 20260619 |
| CPU+SIM L2 | 三阶段 **max=0** |
| 修改 | `f203_ref_common.py`、`f203_encrypt_pack_entry.cpp`、`compress_d_vec.hpp` |
| 误用 | `(1<<27)` on d=5 |
| 正确 | `(1<<26)` on d=5 |

**smoke**：

```bash
bash scripts/build_liboqs_pke_ref.sh
bash scripts/liboqs_pke_vs_ascendc.sh -r cpu -v Ascend910B4
```

---

## 6. 维护

- 新 `Compress_d`（d=4/10 等）接入 Encrypt/Decrypt 时，在本 note §2 扩表并跑 L2。
- liboqs 版本变更时重跑 L2；fixture 目录 `output/liboqs_pke_fixture/<SEED_D>/` 可保留作回归基线。
