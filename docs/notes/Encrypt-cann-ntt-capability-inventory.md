# Encrypt × cann-ntt · 基础能力清单（ML-KEM-1024 / Alg.14）

> **用途**：按 FIPS 203 **K-PKE.Encrypt（Alg.14）** 拆能力；标明「Encrypt 需要什么」与「仓内已有积木在哪」。  
> **主线**：Host 多段编排 + **工程内迁入**的 cann-ntt NTT/INTT；目标 **绝对不卡死**；正确性次要。  
> **禁抄**：整段 PKE/KEM 算子（Encrypt/Encaps/Decaps/KeyGen 全核）**不得参考实现、不得照抄**。  
> **可参考不可抄码**：测过 NTT+编解码等的准全链探针——只读契约/STATUS/笔记，**因 NTT 已换积木，禁止搬源码**。  
> **配套**：[`Encrypt-cann-ntt-kb.md`](Encrypt-cann-ntt-kb.md) · [`Encrypt-cann-ntt-workmode.md`](Encrypt-cann-ntt-workmode.md) · [`../rg-encrypt-cann-ntt.yaml`](../rg-encrypt-cann-ntt.yaml)

**参数锁定**：ML-KEM-1024 → \(k=4,\ n=256,\ q=3329,\ \eta_1=2,\ \eta_2=2,\ d_u=11,\ d_v=5\)。

---

## 1. Alg.14 步骤 → 能力映射

| Alg.14 | 语义（摘要） | 需要的基础能力 | 建议 Host 段 |
|--------|----------------|----------------|--------------|
| 输入 | \(ek_{\mathrm{PKE}}=(\mathbf{t}\Vert\rho),\ m,\ r\) | ByteDecode₁₂、种子布局 | Prep |
| L3–15 | 扩 Â、采样 \(\mathbf{y},\mathbf{e}_1,e_2\) | SHAKE/XOF、SampleNTT、PRF、CBD η₂ | Prep（AIV） |
| L16 | \(\hat{\mathbf{y}}=\mathrm{NTT}(\mathbf{y})\) | **正向 NTT（本线 = 迁入 cann-ntt）** | NTT launch |
| L17–18 | \(\mathbf{u},\ v\) 域运算（Âᵀ∘ŷ + …） | NTT 域乘法/内积（matvec）、模加 | Matvec（AIV） |
| L19 | \(+\ e\)、嵌入 \(m\) | Decompress₁ / μ embed、模加 | Matvec 尾或 Pack 前 |
| L20–21 | \(\mathbf{u}=\mathrm{INTT}(\hat{\mathbf{u}})+e_1\) 等 | **逆向 NTT（cann-ntt 换矩阵）**、模加 | INTT + 加噪 |
| L22–24 | Compress + ByteEncode → \(c\) | Compress₁₁/₅、ByteEncode | Pack（AIV） |

**本线架构原则**：NTT/INTT **单独 launch**（迁入的 cann-ntt 短 MIX）；Prep / Matvec / Pack **尽量 AIV_ONLY**；**禁止**再拼「NTT+GATE+matvec+INTT」单胖 MIX。

---

## 2. 基础能力清单（积木）

图例：**可用**=可当积木参考/链接 shared；**契约参考**=只读笔记与探针 STATUS；**禁抄**=算子级实现；**本线新建**=须迁入或重写。

### 2.1 哈希 / XOF / PRF

| ID | 能力 | FIPS 角色 | 仓内来源（活跃） | 状态 |
|----|------|-----------|------------------|------|
| H1 | Keccak-f[1600] | SHA3/SHAKE 内核 | `library/shared/keccak_f1600_kernel/` | **可用** |
| H2 | SHAKE128/256 XOF（UB） | SampleNTT / PRF 后端 | `library/shared/shake_xof_kernel/`；探针 `ascendc-tests/pass-shake128-ops-math-toy/`、`pass-shake256-ascendc-toy/` | **可用** |
| H3 | 设备 SHA3-256/512、SHAKE256 标量封装 | G/H 等（Encaps 后置） | `keccak_f1600_kernel/fips203_device_sha3.hpp` | **可用**（Encrypt 本阶段可后置） |
| H4 | Host PRF / SE 采样参考 | golden / Host 造数 | `library/shared/fips203_se_sample/` | **可用**（Host） |

### 2.2 采样

| ID | 能力 | FIPS | 仓内来源 | 状态 |
|----|------|------|----------|------|
| S1 | Alg.7 SampleNTT（单 poly） | Â 系数 | `ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg7-sample-ntt-k4/`；笔记 `F203-Alg7-SampleNTT-单poly技术总结.md` | **可用**（参考接口与布局，禁抄进胖核） |
| S2 | Alg.8 CBD η=2 | \(\mathbf{y},e_1,e_2\) | `…/pass-fix-f203-alg8-cbd-eta2-k4/`；笔记 `F203-CBD-eta2-性能优化技术总结.md` | **可用** |

### 2.3 NTT / INTT

| ID | 能力 | FIPS | 仓内来源 | 状态 |
|----|------|------|----------|------|
| N0 | **cann-ntt BAT 矩阵 NTT（本线主积木）** | L16 / L20–21 | 源：`thirdparty/cann-ntt/`（+ 作者包仅作对照）；**迁入目标**：`graph-tests/enc_cann_ntt/` 内自有树 | **本线新建（迁入）** |
| N1 | Tag5T 三段式向量 NTT（旧） | 历史基线 | `pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2` 等 | **禁作本线 NTT 实现来源**（换积木）；数学契约可读 `MLKEM-NTT-实现总结.md` |
| N2 | 8-poly NTT/INTT 批探针 | 批布局参考 | `pass-fix-f203-stage123-ntt-intt-polyvec8-vec/` | **契约参考**；禁抄 Cube/S123 |
| N3 | merged_kyber / 旧 Matmul NTT | — | frozen | **只读 FROZEN**；出门不带码 |

### 2.4 NTT 域乘法 / 内积

| ID | 能力 | FIPS | 仓内来源 | 状态 |
|----|------|------|----------|------|
| M1 | Alg.11/12 MultiplyNTTs | 基乘积木 | `pass-fix-f203-alg11-12-multiplyntts-k4/` | **可用**（AIV 段） |
| M2 | 4×4×1 内积 / 半行内积 | Âᵀ∘ŷ 类 | `pass-fix-f203-alg11-12-innerproduct-k4/`、`…-halfrows/`；笔记 `F203-innerproduct-k4-技术总结.md` | **可用** |
| M3 | q=3329 Barrett / 模加 | 全域 | `library/shared/f203_mod_q/` | **可用** |

### 2.5 编解码 / Compress

| ID | 能力 | FIPS | 仓内来源 | 状态 |
|----|------|------|----------|------|
| C1 | ByteDecode₁₂（ek→t̂） | L 输入 | `library/shared/f203_byte_codec/`；相关 d 探针 | **可用** |
| C2 | ByteEncode_d / ByteDecode_d（d=4/5/10/11） | c 编解码 | `pass-f203-byteencode-d-vec-k4/`、`pass-f203-alg6-bytedecode-d-vec-k4/`；笔记选型文 | **可用** |
| C3 | Compress_d / Decompress_d | L19/L22–24 | `pass-f203-compress-d-vec-k4/`、`pass-f203-decompress-d-vec-k4/`；统一整数 `f203_unified_round/` | **可用** |
| C4 | Encrypt pack 尾（行 20–24） | c₁‖c₂ | `pass-fix-f203-alg14-lines20-22-23-24-encrypt-pack-k4/` | **契约参考**；禁抄与旧 NTT 耦合部分 |

### 2.6 Host / 工程外壳

| ID | 能力 | 用途 | 来源 | 状态 |
|----|------|------|------|------|
| E1 | Kernel 直调 / run.sh / SIM 日志 | 每刀工程壳 | 仓内 `graph-tests` 既有壳；cannbot `ascendc-direct-invoke-template` | **可用** |
| E2 | `acl_session` DeviceGuard | 杀挂后减次生污染 | `library/shared/acl_session/` | **可用** |
| E3 | cannbot sync_audit / deadlock-triage | 每刀同步门禁 | `thirdparty/cannbot-skills/ops/ascendc-sync-audit/` | **强制** |

---

## 3. 明确排除（算子级 · 禁参考实现/禁抄）

| 排除 | 路径示例 | 原因 |
|------|----------|------|
| 全链 PKE Encrypt | `examples/**/…-pke-encrypt-*`、`pass-fix-f203-alg14-pke-encrypt-device-k4` | 旧 MIX 胖核 / 卡死拓扑 |
| Encaps / Decaps / KeyGen 全核 | `examples/**/…-kem-*`、对应 pass | 算子级；非本线积木 |
| Encrypt compute 准全链 | `pass-fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4` 等 | 可作**笔记契约**；**禁止抄核**（NTT 已换） |
| frozen Encrypt / 卡死修补树 | `ascendc-tests/frozen/frozen-*-encrypt*` | 只读判决书 |
| hang 重写 ER01–05 核 | `graph-tests/enc_related/ER0*` | **只读教训**；非实现模板 |

---

## 4. 能力缺口（相对本线目标）

| 缺口 | 说明 | 拟补方式 |
|------|------|----------|
| G1 | 工程内可 launch 的 cann-ntt NTT/INTT | **EN01–02 闭合** |
| G2 | Host 多段 Encrypt 壳 + 贯通 | **EN03–08 闭合**（SIM） |
| G3 | Matvec↔新 NTT 布局 | **EN04/07 初闭合** |
| G4 | 真机不卡死证据 | **EN10 闭合（本路径）**：910B3 上 Host 编排链不挂 |
| G5 | 设备 SampleNTT | **EN09 闭合** |

---

## 5. 使用规则（给主控 / subagent）

1. 开刀前对照本表勾选本刀用到的 ID（H/S/N/M/C/E）。  
2. **N0 未迁入并单测绿之前**，不接真 Prep/Matvec 全量。  
3. 引用积木时：优先 `library/shared/` + 单功能 `pass-*`；**禁止** `#include` 或复制 Encrypt/KEM 算子目录源码。  
4. 准全链探针：只读 `STATUS.md` / `docs/notes/*` 里的 **I/O 与分段语义**，不打开核文件当模板（任务书可写「禁止阅读路径」）。
