# INTEGRATION_PLAN — pass-fix-f203-alg15-pke-decrypt-device-k4

**定位**：FIPS 203 **Algorithm 15 K-PKE.Decrypt**（ml_kem_1024 / k=4）的 **单 AI Core 优化实现探针**。  
相对正确性探针 [`stable-fips203-mlkem-pke-decrypt-k4`](../../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-decrypt-k4/)：本目录追求 **更少 launch / 更少 GM 往返 / 向量化 Compress₁**，最终目标 **SIM 单 kernel launch**；**不**要求第一版即单 kernel。

**标准锚点**：`library/documents/NIST.FIPS.203.pdf` §5.3 Algorithm 15（p.31）；§4.2.1 Compress/Decompress（Eq 4.7–4.8）；Algorithm 5 ByteEncode_d。

**状态**：**PASS**（2026-07-09 改名自 `fix-…`）；单 kernel + 尾融合；SIM ~**283k**。UB 驻留 A/B 实验均回滚（见 STATUS）。

**种子（锁定）**：日常 `SEED_D=20260619`（与 Encrypt / G4 Decrypt round-trip 同源）。

---

## 1. 数学全链（锁定；与标准逐行对齐）

```text
# 输入
dk_PKE ∈ 𝔹^{384k} = 1536B     # ByteEncode₁₂(ŝ)
c      ∈ 𝔹^{32(d_u k + d_v)} = 1568B   # d_u=11, d_v=5；c = c₁ ‖ c₂

# Alg.15
1–2  c₁ ← c[0 : 32·d_u·k]          # 1408B
     c₂ ← c[32·d_u·k : 32(d_u·k+d_v)]  # 160B
3    u' ← Decompress_{11}(ByteDecode_{11}(c₁))   # k=4 poly
4    v' ← Decompress_{5}(ByteDecode_{5}(c₂))     # 1 poly
5    ŝ  ← ByteDecode₁₂(dk_PKE)                   # k=4 poly
6    w  ← v' − NTT⁻¹( ŝᵀ ∘ NTT(u') )
       # 展开：û ← NTT(u')；ŵ ← Σⱼ MultiplyNTTs(ŝ[j], û[j])；w ← v' − INTT(ŵ)
7    m  ← ByteEncode₁(Compress₁(w))              # 32B
8    return m
```

| 不变量 | 说明 |
|--------|------|
| **无采样** | 无 SampleNTT / CBD / PRF / coins |
| **无矩阵 Â** | 仅 ŝ·û 内积（相对 Encrypt 算力更轻） |
| **行 7 方向** | 是 **`ByteEncode₁(Compress₁(w))`**，**不是** `ByteDecode₁`，也不是 Compress 的「逆」 |
| **I/O** | Host 入 `dk_pke`+`c`（+LUT）；出 **仅** `m`（32B）；中间态默认不落盘（调试 Gate 可开） |

**Compress₁ / ByteEncode₁（本探针新写重点）**

| 算子 | FIPS | 定点 / 布局 | 本探针实现选型（已确认） |
|------|------|-------------|--------------------------|
| `Compress₁` | Eq 4.7，`d=1`：`⌈(2/q)·x⌋ mod 2` | liboqs：`((u*1290168)+(1<<30))>>31`；G4 golden：`((x<<1)+(q+1)/2)/q & 1`（须对拍一致） | **尝试向量化**（默认路径） |
| `ByteEncode₁` | Alg.5，`d=1`：256×1bit → 32B | LSB-first：`m[i>>3] \|= bit<<(i&7)` | **标量 pack**（不追求向量 bit 流） |

对照 Encrypt 行 20：`μ ← Decompress₁(ByteDecode₁(m))` 是 **反向** 消息嵌入；**禁止**把 `f203_mu_embed` 当 Decrypt 尾段抄。

---

## 2. 与现有探针的关系

| 角色 | 目录 | 本探针用法 |
|------|------|------------|
| 正确性基线（G4） | [`stable-fips203-mlkem-pke-decrypt-k4`](../../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-decrypt-k4/) | **语义 / golden / round-trip 对照**；2 launch + NTT/INTT 分核已证 SIM 正确；**禁止**把其标量尾当最终优化规格 |
| Encrypt device 样板 | [`pass-fix-f203-alg14-pke-encrypt-device-k4`](../pass-fix-f203-alg14-pke-encrypt-device-k4/) | 编排、自包含、CPU/SIM 分叉、验收权重（SIM 主参考） |
| Compress 向量（d≠1） | [`pass-f203-compress-d-vec-k4`](../pass-f203-compress-d-vec-k4/) | **模式参考**（Barrett / cast_div）；**d=1 未覆盖**，须本目录新写 |
| Decompress / ByteDecode | [`pass-f203-decompress-d-vec-k4`](../pass-f203-decompress-d-vec-k4/) · [`pass-f203-alg6-bytedecode-d-vec-k4`](../pass-f203-alg6-bytedecode-d-vec-k4/) | prep：Decode **标量** + Decompress **向量**（选型笔记已定） |
| NTT/INTT / 内积 | stage123 · Alg.11/12 · G4 `compute/` | vendor_sync **复制**进本目录；禁止跨探针 `#include` |

**抄码规则**：允许从活跃探针 / `library/shared` 复制；**禁止**从 `frozen/` 抄实现或路线。

---

## 3. 优化目标与分期（不一次到位）

### 3.1 最终目标（远期）

| 目标 | 说明 |
|------|------|
| **SIM 单 kernel launch** | 单次 `aclrtLaunchKernel` 完成 Alg.15 全链 → `m` |
| **单 AI Core** | `blockDim=1`（1×AIC + 2×AIV MIX，与 Encrypt compute 同档） |
| **中间态少落盘** | 生产路径仅 D2H `m`；调试 Gate 可 dump |

### 3.2 为何不第一版就单 kernel

G4 已证（见 [`docs/notes/F203-Alg15-Decrypt-2launch编排技术总结.md`](../../docs/notes/F203-Alg15-Decrypt-2launch编排技术总结.md)）：

| 约束 | 后果 |
|------|------|
| prep（AIV decode/decompress）与 NTT 同 launch 且 AIC 立即进 NTT | SIM 上 **û 错** |
| NTT 与 INTT 同 kernel 共用 CrossCore flag 1–3 | **m 错** 或死锁 |
| flag>3 段间握手 | SIM **死锁** |

因此：**分期收敛**；每期 CPU+SIM `m` max=0 后再并段。

### 3.3 分期 Gate

| Phase | 内容 | Launch（SIM 目标） | 验收 |
|-------|------|-------------------|------|
| **P0** | 方案（本文件）+ 目录骨架 | — | 索引登记 |
| **P1** | **尾段积木**：`w[256] int32` → **向量 Compress₁** → **标量 ByteEncode₁** → `m[32]` | 1（AIV-only 或 MIX 占位） | vs golden 全系数 / 32B |
| **P2** | **优化 2-launch**：prep（Decode+Decomp 向量化）‖ compute（NTT+dot+INTT+尾内联） | **2**（compute 内仍可 2 kernel+sync） | `m` max=0；tick ≤ G4 ~427k 为底线，争取更低 |
| **P3** | UB 驻留 / 减少 GM；尝试 NTT–INTT 同 session 更紧编排 | 仍 2 或试验 1 | SIM 正确优先 |
| **P4** | **单 kernel launch**（若 SIM 可证） | **1** | `m` max=0；记 tick；失败则保留 P2/P3 为生产路径 |

**CPU**：允许更多 launch / 标量 fallback 作**辅助正确性**；交付主证据面为 **SIM**（对齐 Encrypt 口径）。

---

## 4. Compress₁ 向量化方案（P1 核心）

### 4.1 数学与定点

```text
Compress₁(u) = round(2u / q) mod 2 ,  u ∈ [0, q), q=3329
```

**Golden 契约（须与 liboqs / G4 一致）**：

```text
# 形式 A（G4 / golden_m.py）
bit = (((u << 1) + (q + 1) / 2) / q) & 1

# 形式 B（liboqs mlk_scalar_compress_d1）
bit = ((u * 1290168u) + (1u << 30)) >> 31
```

P1 写码前用 host 脚本对 `u∈[0,q)` **全量对拍 A↔B**；设备向量路径以 **与 golden 一致** 为准（优先对齐形式 B 的 Barrett，便于 `Muls`/`Adds`/`ShiftRight`）。

### 4.2 向量路径（拟）

对齐 [`docs/notes/F203-Compress-Decompress-向量实现指南.md`](../../docs/notes/F203-Compress-Decompress-向量实现指南.md) **P-COMP-1**，扩展 **d=1**：

| 项 | 值 |
|----|-----|
| 乘数 `M_1` | `1290168`（= `2·round(2³¹/q)`，liboqs） |
| bias | `1<<30` |
| shift | 31 |
| 输出 | int32 lane ∈ `{0,1}`（再交给标量 Encode） |

伪代码（128-wide tile ×2）：

```text
Muls(d0, uTile, 1290168, 128)   # u32 wrap OK
Adds(d0, d0, 1<<30, 128)
ShiftRight(bits, d0, 31, 128)   # 结果 0/1
```

**API 查阅**：写码前查 [`CANN-AscendC算子开发接口参考-查阅索引.md`](../../library/documents/CANN-AscendC算子开发接口参考-查阅索引.md)（`Muls`/`Adds`/`ShiftRight`）；缺则查 PDF 并刷新索引。

**标量 fallback**：`COMPRESS_1_VEC=0` 对照；默认 **`=1`**。

### 4.3 ByteEncode₁（标量，已确认）

```text
for i in 0..255:
  m[i>>3] |= Compress₁(w[i]) << (i & 7)
```

256 bit → 32B；**不做** d=5/11 式向量 pack 实验（收益差，见 ByteEncode 选型笔记）。

---

## 5. 全链编排（P2 起）

### 5.1 推荐生产路径（P2，对齐 G4 切分、优化实现）

```text
aclInit / CreateStream
  │
  ├─ Launch-1: decrypt_prep
  │     ByteDecode₁₁/₅(c) → Decompress → u',v'   # Decode 标量 + Decomp 向量
  │     ByteDecode₁₂(dk) → ŝ
  │     aclrtSynchronizeStream
  │
  └─ Launch-2: decrypt_compute（逻辑一次；内部可 2 device kernel）
        2a  NTT(u')→û；ŝ·û→ŵ；pad
        sync
        2b  INTT(ŵ)→w_time；w←v'−w_time；Compress₁(vec)；ByteEncode₁(scalar)→m
        aclrtSynchronizeStream
  D2H m
```

### 5.2 单 kernel（P4，远期）

仅在 P2/P3 SIM 稳定后尝试：同一 MIX kernel 内用 **已验证** 的段间同步（或 host 不可见的设备内屏障）串起 prep→NTT→INTT→尾。  
**失败判据**：û/m 错、死锁、或 tick 无收益 → **回退 P2/P3**，不硬并。

---

## 6. I/O 与 Golden

| 路径 | 尺寸 | 说明 |
|------|------|------|
| `input/dk_pke.bin` | 1536B | 与 G4 / Encrypt round-trip 同源（`SEED_D`） |
| `input/c.bin` | 1568B | 同上 |
| `input/lut_*.bin` | NTT/INTT LUT | Host 只搬 LUT |
| `output/m.bin` | **32B** | **唯一生产输出** |
| `output/golden_m.bin` | 32B | host_golden；`DECRYPT_VERIFY=1` 对拍 |

Golden 计算块须落在 baseline-registry / 已验证 API（NTT、MultiplyNTTs、Compress₁、ByteEncode₁）；缺项先停。

仓库级对照（**默认 Decrypt = 本目录**）：`scripts/roundtrip_pke_encrypt_decrypt.sh`、`scripts/liboqs_pke_vs_ascendc.sh`。回退 2-launch：`DECRYPT_DIR=.../../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-decrypt-k4`。

---

## 7. 目录骨架（写码时填充；本阶段仅方案）

```text
pass-fix-f203-alg15-pke-decrypt-device-k4/
├── INTEGRATION_PLAN.md      # 本文件
├── STATUS.md
├── SELF_CONTAINED.md        # 写码时补
├── unpack/ / prep/          # Decode + Decompress（P2）
├── compute/                 # NTT / su_dot / INTT / extract
│   └── compress1_byteencode1/   # P1 积木（向量 Compress₁ + 标量 Encode₁）
├── scripts/host_golden/
├── main_*.cpp / CMakeLists.txt / run.sh   # 用户确认写码后再建
└── …
```

---

## 8. 验收命令（写码后）

```bash
cd ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg15-pke-decrypt-device-k4
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4   # 或默认已 SIM_DIRECT=1
```

| 模式 | 角色 |
|------|------|
| **SIM** | **主参考**（同构路径、tick、交付结论） |
| **CPU** | 辅助正确性（可不同构 launch） |

声称 PASS 前：双模式 `m` max=0；用例根无 stray CaModel dump。

---

## 9. 本阶段不做

| 项 | 说明 |
|----|------|
| kernel / CMake / `run.sh` / golden 脚本 | 待用户确认本方案并明确「可以写代码」 |
| 晋级 `examples/incubating` / `stable` | 探针 PASS 后再议（T15a） |
| 从 G4 或 Encrypt **改 d=1 硬套** Compress₅/₁₁ 路径 | d=1 单独实现 |
| 单 kernel 作为 P1 必达 | 仅 P4 尝试 |

---

## 10. 文档与索引

| 文档 | 用途 |
|------|------|
| 本文件 | 实现方案 |
| [`F203-Alg15-Decrypt-2launch编排技术总结.md`](../../docs/notes/F203-Alg15-Decrypt-2launch编排技术总结.md) | 为何分 launch（历史约束） |
| [`F203-Compress-Decompress-向量实现指南.md`](../../docs/notes/F203-Compress-Decompress-向量实现指南.md) | Compress 向量模式；**待补 d=1** |
| [`F203-ByteEncode-ByteDecode-d-向量与标量选型.md`](../../docs/notes/F203-ByteEncode-ByteDecode-d-向量与标量选型.md) | Encode₁ 标量理由 |
| Encrypt 交付口径 | SIM 主参考 / CPU 辅助 |

写码阶段须同步：`STATUS.md`、当日 `qa/`、`ascendc-tests/INDEX.md`、必要时 Compress 指南补 d=1 行。
