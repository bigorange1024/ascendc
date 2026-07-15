# INTEGRATION_PLAN — fix-f203-alg20-kem-encaps-device-k4

**定位**：`ascendc-tests/` **Alg.20 `ML-KEM.Encaps` 设备主线**（**ml_kem_1024 / k=4**）。经 **Alg.17 Encaps_internal** 调用 **stable 对齐**的 Alg.14 Encrypt；**无** `vendor/`、**不**抄 frozen G5。

**基线对照**（只读判决 / STATUS，禁止抄 frozen 源码）：

| 路径 | 角色 |
|------|------|
| [`stable-fips203-mlkem-pke-encrypt-k4`](../../examples/stable/stable-fips203-mlkem-pke-encrypt-k4/) | **Alg.14 Encrypt** 权威（SIM 2 launch） |
| [`pass-fix-f203-alg14-pke-encrypt-device-k4`](../pass-fix-f203-alg14-pke-encrypt-device-k4/) | Encrypt device 探针对照 |
| [`fix-f203-alg20-kem-encaps-correctness-k4`](../fix-f203-alg20-kem-encaps-correctness-k4/) | oracle（vendor G5）；本目录取代其生产接线 |
| [`pass-fix-f203-alg19-kem-keygen-device-k4`](../pass-fix-f203-alg19-kem-keygen-device-k4/) | 供给 `ek_kem`；工程范式（编译期引用 stable） |

**自包含**：[`SELF_CONTAINED.md`](SELF_CONTAINED.md)

---

## 0. 用户锁定（2026-07-15）

| 项 | 约定 |
|----|------|
| **参数集** | ml_kem_1024（**k=4**） |
| **Encrypt 源** | **编译期引用** stable Encrypt 的 `prep/` + `compute/`（对齐 KeyGen device-k4）；**禁止** frozen G5 `vendor_sync` |
| **KEM 头** | `H(ek)` + `G(m‖H(ek))` **并入 prep 入口前段**；**不**另开 kernel launch |
| **`m`** | **GM 输入**（工程按 Alg.17 / derand：`input/m.bin` 32B）；熵从哪来暂非本探针重点 |
| **`coins`/`r`** | **仅** device 内 `G` 后半写入 workspace GM；Host **禁止**预填 `coins.bin` |
| **输出** | `output/c.bin`（1568B）+ `output/K.bin`（32B） |
| **Launch** | **= stable Encrypt**：SIM **2**（prep_kem → l18_l19）；CPU **5**（同 stable 分叉） |
| **SHA3** | `library/shared/keccak_f1600_kernel`：`H`=SHA3-256，`G`=SHA3-512 |

---

## 1. FIPS 代数（相对 Encrypt 的增量）

对外实现可对应 Alg.20 的业务 API；**设备核 I/O 对应 Alg.17**：

```text
输入：ek ∈ B^{1568}，m ∈ B^{32}
(K, r) ← G(m ‖ H(ek))          // 设备 prep 前段
c ← K-PKE.Encrypt(ek, m, r)     // stable Encrypt 余下路径（r 即 coins）
输出：(K, c)
```

| 量 | 字节 | 位置 |
|----|------|------|
| `ek` | 1568 | `input/ek_kem.bin`（= `ek_PKE`） |
| `m` | 32 | `input/m.bin` → `m_gm`（launch2 仍用） |
| `h` | 32 | UB only |
| `K` | 32 | `K_gm` → `output/K.bin` |
| `r`/`coins` | 32 | workspace GM（设备写，不落盘） |
| `c` | 1568 | `output/c.bin` |

---

## 2. Launch 拓扑

```text
Host：读 ek_kem、m、LUT；分配 coins/a_hat/re/K workspace
  │
  ├─ Launch-1: f203_kem_enc_prep     // 本目录 entry（包装 stable prep）
  │     block0: KemEncInitHead(ek,m→K,coins)   // SHA3 头
  │     dual AIV: BuildEncryptPrepSinglePipe   // Â + CBD(re) ← coins
  │
  └─ Launch-2: f203_encrypt_l18_l19  // stable compute+内联 pack
                m_gm + a_hat + re → c
Host：D2H c、K
```

CPU：沿用 stable **5 launch** 分叉；第一 launch 换为 `f203_kem_enc_prep`（头+prep）；后续同 stable（`m`/`coins` 已在 GM）。

---

## 3. 目录与接线（KeyGen device 同型）

| 路径 | 内容 |
|------|------|
| `cmake/encaps/CMakeLists.txt` | `STABLE_ENCRYPT_ROOT` → stable Encrypt；`KERNEL`=`kem/f203_kem_enc_prep_entry.cpp` + stable compute 五段 |
| `kem/f203_kem_enc_*.hpp` | `KemEncInitHead`：读 `m_gm`、`H`、`G`、写 `K`/`coins` |
| `kem/f203_kem_enc_prep_entry.cpp` | 注册 `f203_kem_enc_prep` |
| `main_kem_encaps.cpp` | host 编排（从 stable `main.cpp` 改编） |
| `scripts/gen_data.py` | 备 `ek`（拷 alg19 device 或指定）、`m`、LUT；可选 liboqs golden |
| `scripts/verify_kem_encaps.py` | `c`/`K` vs golden |

**禁止**：`#include` frozen；rsync G5 到 `vendor/`；Host 算 `G`/`H` 冒充设备。

---

## 4. Gate

| Gate | 内容 | 验收 |
|------|------|------|
| **G0** | CMake+run.sh 壳；kernel 结束 | **PASS** |
| **G1** | 注入固定 `m`+`ek`；头后 `coins`/`K` vs host tiny_sha3 | 中间量（隐含于全链；VERIFY 全链绿） |
| **G2** | 全链 `c`/`K` vs liboqs `encaps_derand` | **CPU PASS** max=0（2026-07-15） |
| **G3** | SIM 同 G2；更新 tick 表 | **SIM PASS** max=0；tick **721010**（2026-07-15） |

---

## 5. 验收命令（声称通过前）

```bash
cd ascendc-tests/fix-f203-alg20-kem-encaps-device-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
# 可选：仓库 scripts/liboqs_kem_encaps_batch.sh（接线后改 ENCAPS_DIR）
```

防挂死预算：`KERNEL_COMPUTE_BUDGET_SEC` 默认与 Encrypt 全链同量级（≥600）。

---

## 6. 非目标（本轮）

- 在 AI Core 调系统 CSPRNG
- `#交付#` / `examples/stable` Encaps
- Decaps（T19b/c）
- 改 correctness 目录逻辑
