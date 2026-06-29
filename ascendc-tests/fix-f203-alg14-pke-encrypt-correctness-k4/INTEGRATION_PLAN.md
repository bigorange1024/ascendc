# INTEGRATION_PLAN — fix-f203-alg14-pke-encrypt-correctness-k4

**定位**：`ascendc-tests/` **Alg.14 设备全链拼装探针**——用**已验收 AscendC 积木**多 launch 拼出 `c ← Encrypt(ek, m)`；**非** `examples/` 交付、**非** liboqs / 外部 KEM 黑盒走生产路径。

**自包含约束**：[`SELF_CONTAINED.md`](SELF_CONTAINED.md)（对齐 KeyGen pass 探针教训：外部 oracle 不得渗入默认 `run.sh`）。

**参照 KeyGen 终态**：[`pass-fix-f203-alg13-device-keygen-k4`](../pass-fix-f203-alg13-device-keygen-k4/) · [`stable-mlkem-f203-pke-keygen-k4`](../../examples/stable/stable-mlkem-f203-pke-keygen-k4/)

---

## 1. 目标与不变量

FIPS 203 **Algorithm 14**（ml_kem_1024 / k=4）：

```text
input:  ek_PKE (1568B) = ByteEncode₁₂(t̂) ‖ ρ[32]
        m (32B)
        coins (32B)  → 内部 KDF 得 r, e₁, e₂
output: c (1568B) = c₁ ‖ c₂
```

| 不变量 | 说明 |
|--------|------|
| **设备全链** | 默认 `run.sh` 密码学在 **AI Core**；Host 只写种子类 input、读 output |
| **拼装来源** | 仅 **活跃探针** vendored 到本目录 + `library/shared`；禁止 `#include` 其它探针路径 |
| **Golden 角色** | `scripts/host_golden/` **仅** `ENCRYPT_VERIFY=1` 或分阶段 gate；不得作为生产输入源 |
| **禁止** | `thirdparty/liboqs`、`oqs.h`、`PQCP_MLKEM_*` 进入本探针 `run.sh` / `main` / 设备核 |

---

## 2. 已验收积木（拼装清单）

| Alg.14 段 | FIPS | 活跃探针 | SIM tick（参考） | 备注 |
|-----------|------|----------|------------------|------|
| **L1** `Â` from ρ | 行 3–7 / Alg.7×16 | [`pass-fix-f203-alg13-lines3-7-a-hat-k4`](../pass-fix-f203-alg13-lines3-7-a-hat-k4/) | ~381k | `a_hat[16,256]` int32 GM |
| **L2** `r,e₁,e₂` | 行 8–15 / Alg.8+12 | [`pass-fix-f203-alg13-lines8-15-se-k4`](../pass-fix-f203-alg13-lines8-15-se-k4/) | — | Encrypt 用 **η₁/η₂** 分支（非 KeyGen η） |
| **L3** NTT / INTT | 行 16–17 | [`pass-fix-f203-stage123-ntt-intt-polyvec8-vec`](../pass-fix-f203-stage123-ntt-intt-polyvec8-vec/) | NTT 30347 / INTT 30340 | poly-batch；**S1–S3 禁 Gather** |
| **L4** `Âᵀ·r̂` → `û` | 行 18 | vendored [`innerproduct-k4`](../pass-fix-f203-alg11-12-innerproduct-k4/) | 43992 | **非新探针**：同 GM `a_hat[16,256]`，读 `A[j,p]` 即 `a_hat_offset(j,p)`（见 §2.1） |
| **L5** `t̂·r̂` + 加 `e₁` | 行 18–19 | vendored [`vec-k4-v2`](../pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/) 内积 UB + 噪声加 | 77958 全链 | `t̂` 自 `ek` ByteDecode₁₂（§2.2） |
| **L6** `μ` 嵌入 + `e₂` | 行 20–21 | vendored lines8-15 / Alg.8 CBD | — | |
| **L7** INTT `u,v` | 行 21–22 | L3 INTT | — | |
| **L8** `Compress_d` + `ByteEncode_d` | 行 22–23 | vendored [`compress-d`](../fix-f203-compress-d-vec-k4/) + [`byteencode-d`](../fix-f203-byteencode-d-vec-k4/) | d=4/10 已有 | ml_kem_1024：**d=11/5** = CMake 常数扩展（§2.3） |

### 2.1 `Â` 与 `Âᵀ`：同一 16 poly GM，仅索引对调

GM 布局与 KeyGen 一致（`innerproduct_layout.h`）：

```text
A[p,j] @ flat = (p*K + j) * N + c     K=4, N=256
```

KeyGen 行 18：`t̂[p] = Σ_j MultiplyNTTs( A[p,j], ŝ[j] )` → 读 `a_hat_offset(p, j)`。

Encrypt：`û[p] = Σ_j MultiplyNTTs( A[j,p], r̂[j] )`（即 `Âᵀ[p,·]·r̂`）→ 读 **`a_hat_offset(j, p)`**。

```text
Âᵀ[p,j] = A[j,p]   →   aT[p,j] 的 local poly 就是 GM 里 a[j,p]
```

**实现**：vendored `innerproduct_kernel` / `compute_on_ub` 循环结构不变，仅 `DataCopy` 源偏移由 `(p,j)` 改为 `(j,p)`；或封装 `a_hat_offset_at(p,j) = a_hat_offset(j,p)`。**无需**新探针、无需 GM 转置写回。

`t̂·r̂`（标量 poly 输出）为 **k 元内积** `Σ_j t̂[j]∘r̂[j]`，与 L4 同 basemul 原语、外循环为单 poly 累加（vendored vec-k4-v2 行 18 子集）。

### 2.2 `ek_pke` 解析（非阻塞）

| 块 | 尺寸 | 做法 |
|----|------|------|
| `t̂` polyvec | 4×384 B | vendored [`alg6-bytedecode-d`](../fix-f203-alg6-bytedecode-d-vec-k4/) 的 **d=12** 路径（与 KeyGen ByteEncode₁₂ 互逆） |
| `ρ` | 32 B | `ek_pke[1536:1568]`，供 L1 SampleNTT |

### 2.3 `d_u=11` / `d_v=5`（非阻塞）

[`compress-d`](../fix-f203-compress-d-vec-k4/IMPLEMENTATION_PLAN.md) 已列 Barrett 常数（d=11/5）；[`byteencode-d`](../fix-f203-byteencode-d-vec-k4/) 为参数化 pack。拼装时 vendored 拷贝 + `F203_COMPRESS_D` / `F203_BYTE_ENCODE_D` 编译开关即可，与 d=4/10 同模板。

**ek 输入**（生产已有 `ek_pke.bin`）：

| 块 | 尺寸 | 来源 |
|----|------|------|
| `t̂` polyvec | 4×384 B | §2.2 ByteDecode₁₂ |
| `ρ` | 32 B | `ek[1536:1568]` |

---

## 3. 分阶段 Gate（仅设备拼装）

| Gate | 设备路径 | Host 允许 | 验收 |
|------|----------|-----------|------|
| **G0** launch 壳 | marker AIV×1 | 写 `ek,m,coins`；**无**密码学 | CPU/SIM kernel 正常结束 |
| **G1** prep | L1 + L2（2 launch） | 仅种子 | 中间 GM / staging 对 `host_golden` 分段 |
| **G2** NTT 域 | L3 on `r̂` | — | `r̂` vs golden |
| **G3** 线性层 | L4–L6（Âᵀ 索引 `(j,p)` + t̂·r̂ + 噪声） | — | 中间 `û`/`v` vs golden |
| **G4** 落盘 | L7 + L8 | — | `c.bin` 1568B `max=0` |
| **G5** 生产 I/O | 单次或少量 launch 合并 | `input/` 仅 `ek,m,coins` | 同 G4；无 staging 依赖 |

**当前**：**G4**（全链至 `c.bin`）；**G5** 合并 launch 待做。

---

## 4. Launch 编排（终态草图）

```text
input/ek_pke.bin, m.bin, coins.bin
  │
  ├─ Launch-1  f203_encrypt_prep_a_hat     ← vendored from lines3-7（ρ from ek tail）
  ├─ Launch-2  f203_encrypt_prep_re        ← vendored from lines8-15（coins→r,e₁,e₂）
  ├─ Launch-3  f203_encrypt_ntt_r          ← vendored stage123（polyvec NTT）
  ├─ Launch-4  f203_encrypt_at_r           ← innerproduct：`a_hat_offset(j,p)` 读 Âᵀ
  ├─ Launch-5  f203_encrypt_t_dot_r        ← fork vec-k4-v2 hat_dot（t̂ from ek decode）
  ├─ Launch-6  f203_encrypt_add_noise       ← +e₁, embed μ, +e₂
  ├─ Launch-7  f203_encrypt_intt_uv        ← INTT
  └─ Launch-8  f203_encrypt_pack           ← compress_d + byteencode_d → c.bin
```

合并策略（性能阶段，G5 之后）：prep 双 AIV 并行（同 KeyGen）；compute 按 UB 预算合并 launch，**不**牺牲 poly-batch 语义。

---

## 5. 目录骨架（演进）

```text
fix-f203-alg14-pke-encrypt-correctness-k4/
├── INTEGRATION_PLAN.md
├── SELF_CONTAINED.md
├── STATUS.md
├── prep/                    # G1：vendored Â + re
├── compute/                 # G2–G7：NTT / dot / noise / INTT
├── pack/                    # G8：compress + byteencode（d=11/5）
├── scripts/
│   ├── gen_data.py          # 生产 input only
│   ├── verify_result.py
│   └── host_golden/         # 分阶段 golden（抄自探针 ref，禁 liboqs）
├── f203_encrypt_marker_custom.cpp   # G0
└── main_encrypt.cpp
```

**vendored 规则**：从活跃探针 **复制** 源文件到本目录子树；改 include 为相对路径；`rg` 不得命中 `ascendc-tests/pass-fix`（文档链接除外）。

---

## 6. Golden 与验证

```bash
# 默认：仅设备 launch 壳（G0）
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4

# 字节对拍（须 host_golden 已覆盖该 gate）
ENCRYPT_VERIFY=1 bash run.sh -r cpu -v Ascend910B4
```

| 脚本 | 用途 |
|------|------|
| `scripts/host_golden/` | 从本目录 vendored ref **拼装**期望；可含**原样抄写**的 C ref，**不得**链 liboqs |
| `scripts/verify_result.py` | `ENCRYPT_VERIFY=1` 时 `c.bin` vs `golden_c.bin` |

**审查**（防 KeyGen 式残留）：

```bash
rg -i 'liboqs|oqs\.h|PQCP_MLKEM|indcpa_enc' --glob '!host_golden/README.md'
rg '#include.*ascendc-tests/(pass|fix)-' prep compute pack *.cpp
```

---

## 7. 后继（无结构性阻塞）

| 项 | 说明 |
|----|------|
| **Âᵀ·r̂** | vendored innerproduct，偏移 `(j,p)` 对调（§2.1） |
| **d=11/5** | vendored compress/byteencode + 编译常数（§2.3） |
| **ek → t̂** | vendored ByteDecode₁₂ d=12（§2.2） |
| **定型交付** | 另建 `examples/incubating/exp-mlkem-f203-pke-encrypt-k4`（须 customspec） |

**当前工程顺序**：G1 prep vendored → G2 NTT → G3 线性层（含 Âᵀ 索引）→ G4 pack → G5 合并 launch。

---

## 8. 与 KeyGen 的差异（易混）

| | KeyGen Alg.13 | Encrypt Alg.14 |
|--|---------------|----------------|
| 噪声 | η, η, η (s,e) | η₁ (r,e₁), η₂ (e₂) |
| 矩阵乘 | `Â·ŝ`：读 `A[p,j]` | `Âᵀ·r̂`：读 `A[j,p]`（**同 GM**，索引对调） |
| 输出 pack | ByteEncode₁₂ → ek | Compress+ByteEncode **d_u/d_v** → c |
| 已有全链 | pass keygen ✅ | **无**；本探针目标 |
