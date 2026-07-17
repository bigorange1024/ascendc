# INTEGRATION_PLAN — fix-f203-alg21-kem-decaps-device-k4

**定位**：`ascendc-tests/` **Alg.21 `ML-KEM.Decaps` 无 vendor 设备主线**（**ml_kem_1024 / k=4**）。内部 **Alg.18 `Decaps_internal`**；PKE 段 **编译期引用** stable Decrypt / Encrypt（或 Encaps 树），**禁止** frozen G4/G5 `vendor_sync`。

**基线对照**（只读判决 / STATUS；**禁止**抄 frozen 源码）：

| 路径 | 角色 |
|------|------|
| [`stable-fips203-mlkem-pke-decrypt-k4`](../../examples/stable/stable-fips203-mlkem-pke-decrypt-k4/) | Phase-D Decrypt 权威 |
| [`stable-fips203-mlkem-pke-encrypt-k4`](../../examples/stable/stable-fips203-mlkem-pke-encrypt-k4/) / [`stable-…-kem-encaps-k4`](../../examples/stable/stable-fips203-mlkem-kem-encaps-k4/) | Phase-E Encrypt 权威 |
| [`pass-fix-f203-alg20-kem-encaps-device-k4`](../pass-fix-f203-alg20-kem-encaps-device-k4/) | Encaps「头融 prep」工程范式 |
| [`fix-f203-alg21-kem-decaps-correctness-k4`](../fix-f203-alg21-kem-decaps-correctness-k4/) | oracle（vendor G4+G5）；本目录取代其生产接线 |
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
| **SIM session** | 首版允许 `ASCENDC_SIM_HOST_MODE=decaps_2session`（D 后 fresh session 跑 E）；单 session 真修属 T2 |
| **单设备库** | **CPU：单** `libascendc_kernels_cpu.so`。**SIM：双库**（`…_dec_sim` + `…_sim`）+ **强制 2-session**——stable Decrypt/Encrypt 同名头（`aiv_func`/`ntt_vec` 等）在 ascendc precompile 无法 per-TU 隔离；合库单 session 属 **T2** |

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
  │     SIM: f203_encrypt_l18_l19（内联 pack → c'）
  │     CPU: ntt / at_jp / intt / …
  │
  └─ FO：目标嵌在 pack 尾（与 c' 同核）
        首版过渡（门禁允许）：CPU 用 probe-local pack+FO 核替代
        `f203_encrypt_alg14_pack`；SIM 若尚未 fork l18_l19，
        可在内联 pack 后 **同 Phase-E 内** 追加一次 AIV-only FO 核
        （不另占「独立 KEM 功能产品名」；下一迭代收回为 l18_l19 尾内联）
Host：D2H K（Phase-E-only 亦可 dump c' 供诊断）
```

**禁止**：为 G 或 FO 再开「与 Encrypt 无关」的第三套产品化 launch 拓扑；Host 算 G/J 写 `K.bin`。

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
| **F1** | D→E 串联；SIM 默认 2-session | `K` max=0 | **PASS**（CPU；SIM D**283317**+E**745341**） |
| **F2** | 分项 kat / roundtrip | 按 scripts 约定 | **PASS**（2026-07-17；默认已指本目录；CPU KAT×1） |

---

## 5. 验收命令

```bash
cd ascendc-tests/fix-f203-alg21-kem-decaps-device-k4
bash run.sh -r cpu -v Ascend910B4          # 默认全链
bash run.sh -r sim -v Ascend910B4          # 默认 decaps_2session
# 调试 Phase-E-only / 拒绝（非默认）：
KEM_DECAPS_PHASEE_ONLY=1 bash run.sh -r cpu -v Ascend910B4
KEM_DECAPS_REJECT=1 bash run.sh -r cpu -v Ascend910B4   # E3：假密文；K vs liboqs
```

防挂死预算：`KERNEL_COMPUTE_BUDGET_SEC`≥600（全链默认 1200）。

---

## 6. 非目标 / 后继

| 项 | 归属 |
|----|------|
| **T2：SIM 单库合库 + 单 session** | **交 Cloud Agent**（见仓库根 `AGENT_HANDOFF.md`）；本机已推送双库+2-session 绿基线 |
| D+E 单 launch 融合 | 更后；非 T2 必做 |
| `#交付#` / `examples/stable` Decaps | T2 或 `pass-fix` 更名之后 |
| 改 correctness 逻辑；从 frozen 抄 PKE | **禁止** |
| `pass-fix` 更名 / KAT 扩量 | 可本机；非阻塞 T2 |

### T2 实施要点（给 Cloud）

1. **问题**：SIM 现双库是因为 Decrypt/Encrypt 同名头在 ascendc precompile 无法 per-TU 隔离；双库同 session 历史上有 func_key 污染 → 强制 `decaps_2session`。
2. **做法候选**：命名空间/路径隔离头、生成侧重命名、或拆 TU 后仍链进**一个** `ascendc_library`；合库后再试 `decaps_1session`。
3. **验收**：CPU 仍绿；SIM 全链 `K` max=0；`KEM_DECAPS_REJECT=1` 仍绿；用例根无 stray dump。
4. **勿**：改 golden 语义绕过；从 `frozen/` 抄码；擅自改已锁形状/`blockDim`。
