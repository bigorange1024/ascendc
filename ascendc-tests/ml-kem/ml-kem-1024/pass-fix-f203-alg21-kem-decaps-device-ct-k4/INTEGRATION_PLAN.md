# INTEGRATION_PLAN — pass-fix-f203-alg21-kem-decaps-device-ct-k4

**定位**：`ascendc-tests/` **Alg.21 `ML-KEM.Decaps` 无 vendor 设备主线**（**ml_kem_1024 / k=4**）。内部 **Alg.18 `Decaps_internal`**；PKE 段 **编译期引用** stable Decrypt / Encrypt（或 Encaps 树），**禁止** frozen G4/G5 `vendor_sync`。

**基线对照**（只读判决 / STATUS；**禁止**抄 frozen 源码）：

| 路径 | 角色 |
|------|------|
| [`stable-fips203-mlkem-pke-decrypt-k4`](../../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-decrypt-k4/) | Phase-D Decrypt 权威 |
| [`stable-fips203-mlkem-pke-encrypt-k4`](../../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-encrypt-k4/) / [`stable-…-kem-encaps-k4`](../../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-encaps-k4/) | Phase-E Encrypt 权威 |
| [`pass-fix-f203-alg20-kem-encaps-device-k4`](../pass-fix-f203-alg20-kem-encaps-device-k4/) | Encaps「头融 prep」工程范式 |
| 历史 correctness oracle | 仍活跃只读对照 [`fix-…-decaps-correctness-k4`](../fix-f203-alg21-kem-decaps-correctness-k4/)（**禁止抄码**；非 frozen 幽灵路径） |
| [`docs/notes/F203-KEM-Alg21-Decaps设备全链与SIM单session技术总结.md`](../../docs/notes/F203-KEM-Alg21-Decaps设备全链与SIM单session技术总结.md) | FO / SIM session 定论 |

**自包含**：[`SELF_CONTAINED.md`](SELF_CONTAINED.md)

---

## 0. 用户锁定（2026-07-17）

| 项 | 约定 |
|----|------|
| **参数集** | ml_kem_1024（**k=4**） |
| **开工顺序** | **先 Phase-E（Alg.18 行 6–12），后 Phase-D（行 1–5）**；两段绿后再谈 D∥E 合段 |
| **行 1–4** | 从 `dk_kem` 切 `dk_pke`/`ek`/`h`/`z` → **并入 Decrypt 入口**；不另开 launch |
| **行 5** | `m' ← Decrypt` → Phase-D 主体（stable Decrypt fused） |
| **行 6** | `(K', r') ← G(m' ‖ h)` → **并入 Encrypt prep 前段**（对齐 Encaps 的 H/G；**不**挂在 Decrypt 尾） |
| **行 7** | `c' ← Encrypt(ek, m', r')` → Phase-E 主体（stable Encrypt） |
| **行 8–12** | `c ≟ c'`；`K ← K'` 或 `J(z ‖ c)` → **并入 Encrypt pack 尾（设备 FO）**；禁止 Host memcmp 冒充生产 FO |
| **首版全链 launch** | **2**：Phase-D + Phase-E（各内部 = 对应 stable 的 launch 数）；**零额外独立 KEM AIV 核名** |
| **最终目标** | 尽可能少 launch；**门禁不是**一上来 D+E 单核 |
| **PKE 源** | 编译期引用 stable；**禁止** `#include` / rsync frozen |
| **SIM session** | **默认 `decaps_2session`**（教材第7章 CT / 本探针生产）；排障 `ASCENDC_SIM_HOST_MODE=decaps_1session` |
| **单设备库** | **CPU/SIM 均为单** `libascendc_kernels_*.so`。SIM 用 `scripts/prepare_dec_shim.sh`（冲突头 `dec_*`）合库；见 §T2 |

---

## 1. FIPS 代数（Alg.21 → Alg.18）

```text
输入：dk ∈ B^{3168}，c ∈ B^{1568}
dk = dk_pke(1536) ‖ ek(1568) ‖ h(32) ‖ z(32)

m'           ← K-PKE.Decrypt(dk_pke, c)     // 行 5（Phase-D）
(K', r')     ← G(m' ‖ h)                    // 行 6（Phase-E prep 头）
c'           ← K-PKE.Encrypt(ek, m', r')    // 行 7（Phase-E Encrypt）
K            ← K'  if c = c' else J(z ‖ c)  // 行 8–12（pack 尾 FO）
输出：K ∈ B^{32}
```

| 量 | 字节 | Phase-E-only 输入（本轮） | 全链时来源 |
|----|------|---------------------------|------------|
| `m'` | 32 | `input/m_prime.bin`（Host 灌，模拟 Decrypt 输出） | Phase-D GM |
| `h` | 32 | `input/h.bin`（自 `dk_kem`） | dk 切片 |
| `z` | 32 | `input/z.bin` | dk 切片 |
| `ek` | 1568 | `input/ek_kem.bin` | dk 切片 |
| `c` | 1568 | `input/c.bin`（FO 比对用） | 调用方密文 |
| `K'` / `r'` | 32+32 | 设备 G 写 workspace（禁止 Host 预填 r'） | 同左 |
| `c'` | 1568 | workspace / 可选 dump | Encrypt 输出 |
| `K` | 32 | `output/K.bin` | FO 输出 |

---

## 2. Launch 拓扑（锁定）

### 2.1 全链目标（D 绿 + E 绿之后）

```text
Host：读 dk_kem、c、LUT
  │
  ├─ Phase-D：stable Decrypt（入口前段切 dk）→ m'_gm
  │            （SIM 可选 aclFinalize → fresh session）
  │
  └─ Phase-E：见 §2.2
Host：D2H K
```

### 2.2 Phase-E（本轮开工 · Alg.18 行 6–12）

对齐 Encaps device：**prep 前段并入 G**；Encrypt 余下同 stable。

```text
Host（Phase-E-only）：灌 m'/h/z/ek/c + LUT；分配 K'/coins/a_hat/re/c'/K workspace
  │
  ├─ Launch-E1: f203_kem_dec_phase_e_prep
  │     block0: KemDecPhaseEHead(m',h → K',coins)   // SHA3-512 G
  │     dual AIV: BuildEncryptPrepSinglePipe          // Â + CBD ← coins
  │
  ├─ Launch-E2…: 同 stable Encrypt compute
  │     SIM: f203_encrypt_l18_l19（内联 pack → c'；**T19i 后同核 FO → K**）
  │     CPU: ntt / at_jp / intt / pack_fo
  │
  └─ FO：
        **目标（T19i）**：嵌在 SIM `l18_l19` pack 尾（与 c' 同核）→ 全链 SIM **3** launch
        **过渡（已落地、待收回）**：SIM 追加 `f203_kem_dec_fo_only`（第 4 launch）
        CPU：probe-local `f203_kem_dec_pack_fo`（pack+FO；保持 6 launch）
Host：D2H K（Phase-E-only 亦可 dump c' 供诊断）
```

**禁止**：为 G 或 FO 再开「与 Encrypt 无关」的第三套产品化 launch 拓扑；Host 算 G/J 写 `K.bin`。

> **现行 SIM**：D + E1 prep + E2 `l18_l19` + E3 `fo_only` = **4**。见 §7 T19i。

---

## 3. 目录与接线

| 路径 | 内容 |
|------|------|
| `cmake/decaps/CMakeLists.txt` | `STABLE_ENCRYPT_ROOT`；KERNEL=`kem/` Phase-E 入口 + stable compute；**单库** |
| `kem/f203_kem_dec_layout.h` | 尺寸常量 |
| `kem/f203_kem_dec_phase_e_init.hpp` | `KemDecPhaseEHead`：`G(m'‖h)`→`K'`+`coins` |
| `kem/f203_kem_dec_fo.hpp` | `KemDecFo`：c vs c'、J、选 K |
| `kem/f203_kem_dec_phase_e_prep_entry.cpp` | 注册 `f203_kem_dec_phase_e_prep` |
| `kem/f203_kem_dec_pack_fo_entry.cpp` | CPU pack + FO（及/或 SIM 过渡 FO 核） |
| `main_kem_decaps_phase_e.cpp` | Phase-E-only host |
| `scripts/gen_data_phase_e.py` | stash ek/dk + liboqs encaps → 灌 m'/h/z/c + golden K |
| `scripts/verify_kem_decaps.py` | `K` vs golden |

Phase-D：`main_kem_decaps_phase_d_run.*` + stable `f203_decrypt_device_fused`；全链 `main_kem_decaps.cpp` + `scripts/gen_data.py`。

---

## 4. Gate

| Gate | 范围 | 验收 | 状态（2026-07-17） |
|------|------|------|---------------------|
| **E0** | CMake + run.sh Phase-E-only；kernel 结束 | 无挂死 | **PASS** |
| **E1** | 合法路径 Phase-E-only | CPU `K` max=0 | **PASS** |
| **E2** | 同 E1 SIM | SIM `K` max=0；记 tick | **PASS**（tick **746221**） |
| **E3** | 拒绝路径：随机假密文 → `K` vs liboqs Decaps≡`J(z‖c)` | CPU（+SIM）max=0 | **PASS**（2026-07-17；`KEM_DECAPS_REJECT=1`） |
| **D0–D2** | Phase-D / 全链内 Decrypt | `m'` 可用；全链 `K` max=0 | **PASS**（并入 F1） |
| **F1** | D→E 串联；SIM 默认 **2-session**（CT；单库） | `K` max=0 | **PASS**（2026-07-24：D**286798**+E**763663**） |
| **F2** | 分项 kat / roundtrip | 按 scripts 约定 | 可选（本轮未跑长测） |
| **T2** | SIM 单库 + 可选 1-session 排障 | 无双 `.so`；`K` max=0 | **合库 PASS**；本探针生产默认仍 **2-session** |

---

## 5. 验收命令

```bash
cd ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg21-kem-decaps-device-ct-k4
bash run.sh -r cpu -v Ascend910B4          # 默认全链
bash run.sh -r sim -v Ascend910B4          # 默认单库 + decaps_2session
# 调试 Phase-E-only / 拒绝 / 1-session 排障（非默认）：
KEM_DECAPS_PHASEE_ONLY=1 bash run.sh -r cpu -v Ascend910B4
KEM_DECAPS_REJECT=1 bash run.sh -r cpu -v Ascend910B4   # E3：假密文；K vs liboqs
ASCENDC_SIM_HOST_MODE=decaps_1session bash run.sh -r sim -v Ascend910B4  # 排障
```

防挂死预算：`KERNEL_COMPUTE_BUDGET_SEC`≥600（全链默认 1800）。

---

## 6. 非目标 / 后继

| 项 | 归属 |
|----|------|
| **T2：SIM 单库合库** | **PASS**：`prepare_dec_shim.sh` + 单 `ascendc_library`；本探针生产默认仍 `decaps_2session`（CT）；`decaps_1session` 为排障 |
| **T19i：SIM `fo_only` → `l18_l19` 尾（4→3）** | **本探针 PASS**（2026-07-20；D**287037**+E**763886**）；stable Decaps 待镜像 |
| D+E 单 launch 融合 | 更后 |
| `#交付#` / `examples/stable` Decaps | **已完成**（2026-07-20）；本探针仍作行为基线 |
| 改 correctness 逻辑；从 frozen 抄 PKE | **禁止** |
| 改共享 PKE Encrypt / Encaps 的 `l18_l19` 源 | **禁止**（见 §7.2） |

---

## 7. T19i — SIM `fo_only` 收回 `l18_l19` 尾（SIM 4→3）

**背景**：当前全链 SIM 为 4 launch（D + prep + `l18_l19` + `fo_only`）。`l18_l19` 已内联 Encrypt pack 写出完整 `c'`；`f203_kem_dec_fo_only` 仅调 `KemDecFo`（过渡核）。目标与 INTEGRATION §0「行 8–12 并入 Encrypt pack 尾」对齐。

### 7.1 目标拓扑

```text
SIM 全链（生产）：
  Launch-D:  f203_decrypt_device_fused
  Launch-E1: f203_kem_dec_phase_e_prep
  Launch-E2: f203_encrypt_l18_l19   // pack → c' 后同核 KemDecFo → K
  （删除 Launch-E3 fo_only）

CPU（不变）：仍 6 launch；末核 f203_kem_dec_pack_fo（pack+FO）
```

### 7.2 隔离策略（强制，避免误伤 Encaps/Encrypt）

本探针 CMake 现将 `COMPUTE_DIR` 指到 [`stable-…-pke-encrypt-k4/compute`](../../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-encrypt-k4/compute/)。**禁止**在该共享树上改 `f203_encrypt_l18_l19_kernel.cpp` 签名或行为（Encaps pass-probe / PKE Encrypt 同编此文件）。

| 做法 | 说明 |
|------|------|
| **采用** | 探针本地覆盖：`kem/f203_encrypt_l18_l19_kernel.cpp`（自 Encrypt 拷贝后仅本目录改）；CMake 对该 TU 改指本地路径 |
| **禁止** | 直接改 `examples/stable/…-pke-encrypt-k4/compute/f203_encrypt_l18_l19_kernel.cpp` |
| **后续** | stable Decaps 自有 vendored `compute/`，镜像本改时只动 Decaps 树 |

### 7.3 设备实现要点

1. **签名**：在现有 `cGm`/`traceGm` 后追加可选 `cInGm,zGm,KprimeGm,KoutGm`；四指针**皆非空**才跑 FO（Encrypt 纯路径若误链本 TU 传空则跳过）。
2. **落点**：`tail_pack_shard_gm(...)` 之后（AIV 分支末尾）。
3. **同步**：双 AIV 均 `AscendC::SyncAll</*isAIVOnly=*/true>()`（pack 分片写完再 FO；API 查阅索引 2026-07-13）。AIC 已结束 INTT，不参与 SyncAll/FO。
4. **FO**：`!aic && subBlockID==0` 调 `F203KemDec::KemDecFo(cIn,c',z,K',K)`（`kem/f203_kem_dec_fo.hpp`，无新矢量 API）。
5. **Host**：`main_kem_decaps_phase_e_run.cpp` SIM 路径删 `ACLRT_LAUNCH_KERNEL(f203_kem_dec_fo_only)`；把四指针并入 `l18_l19` launch。
6. **入口清理**：SIM 不再注册/launch `f203_kem_dec_fo_only`；CPU 仍编 `f203_kem_dec_pack_fo`。

### 7.4 验收（本探针）

| 门禁 | 命令 / 判据 |
|------|-------------|
| CPU 全链 | `bash run.sh -r cpu -v Ascend910B4` → `K` max=0；仍 6 launch |
| SIM 全链 | `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` → `K` max=0；**3** launch；根无 stray dump |
| 拒绝 | `KEM_DECAPS_REJECT=1` cpu+sim → `K`≡`J(z‖c)` |
| tick | 记 D+E；期望不劣于过渡期 D**286803**+E**745925** 同量级 |
| 回归 | 可选：`liboqs_kem_decaps_batch` / roundtrip（`DECAPS_DIR` 指本目录时） |

### 7.5 非本轮

- 不改 examples stable/exp Decaps（待本探针绿后再 `$规格$`/`#修改#`）
- 不改 Encaps / PKE Encrypt
- 不做 D+E 单 launch

### 7.6 本探针落地证据（2026-07-20）

| 项 | 结果 |
|----|------|
| 本地 TU | `kem/f203_encrypt_l18_l19_kernel.cpp`（CMake 改指；未改 Encrypt 共享树） |
| Host | 删 `fo_only`；`l18_l19(..., cIn,z,K',K)` |
| CPU 全链 | **PASS**（`K` max=0；仍 6 launch） |
| SIM 全链 | **PASS**；tick D**287037**+E**763886**；3 launch |
| E3 拒绝 | CPU+SIM **PASS** |

### T2 落地要点（已完成）

1. **问题**：stable Decrypt/Encrypt 同名头 → ascendc precompile 无法 per-TU 隔离 → 曾被迫 SIM 双库 + `decaps_2session`。
2. **做法**：`scripts/prepare_dec_shim.sh` 自 stable Decrypt 复制可达子树，冲突 basename → `dec_*` 并改写 `#include`；SIM 单 `ascendc_library` 吃 shim + Encrypt + kem。
3. **验收**：CPU 仍绿；SIM 全链 `K` max=0（D**286803**+E**745925**）；`KEM_DECAPS_REJECT=1` CPU+SIM 仍绿；仅 `libascendc_kernels_sim.so`；用例根无 stray dump。
4. **勿**：改 golden 语义绕过；从 `frozen/` 抄码；擅自改已锁形状/`blockDim`。
