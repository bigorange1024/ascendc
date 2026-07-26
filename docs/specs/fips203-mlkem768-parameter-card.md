# FIPS 203 ML-KEM-768 参数卡（P0 锁定）

**状态**：**已锁定**（2026-07-26 用户确认 §决议）  
**参数组**：ML-KEM-768（\(k=3\)）  
**范围**：P0 文书；**尚未**写 AscendC kernel  
**完整计划**：[docs/research/MLKEM-768-从0到exp完整实现计划.md](../research/MLKEM-768-从0到exp完整实现计划.md)  
**P1 用例表**：[fips203-mlkem768-p1-gap-and-cases.md](fips203-mlkem768-p1-gap-and-cases.md)

---

## 0. 用户决议（2026-07-26）

| # | 议题 | 决议 |
|---|------|------|
| 1 | 分核主路线 | **T-B**：噪声侧 **polyvec6**（s‖e）；\(\hat A\)（\(3\times3\)）**独立 prep launch** |
| 2 | KEM device 探针 | **保留** D19–D21 |
| 3 | reject / CT | **要求** `…-decaps-device-ct-k3` 与 incubating `…-decaps-ct-k3` |
| 4 | PKE exp | **要做**（keygen / encrypt / decrypt 三个 `exp-…-pke-*-k3`） |
| 5 | 命名后缀 | **`-k3`** |
| 6 | 本轮范围 | **先完成 P0 + P1**（文书 + 目录壳；不写 kernel） |

附加锁定（计划默认，随 P0 一并生效）：

| 项 | 锁定 |
|----|------|
| Compress/ByteEncode 树策略 | **C-1**：768 树自建探针，默认 \(d\in\{10,4\}\)（密钥域另用 d=12） |
| 零垫 | **禁止**（前提 4） |
| stable-768 | **本阶段不建** |
| Launch | CPU 可多分段；SIM/生产少 launch；首版正确性优先，STATUS 标非性能基线 |

---

## 1. FIPS / liboqs 标量参数（已与头文件对拍）

| 符号 | 值 | 核对 |
|------|-----|------|
| \(n\) | 256 | FIPS 203 |
| \(q\) | 3329 | FIPS 203 |
| \(k\) | **3** | FIPS 203 Table 2 |
| \(\eta_1,\eta_2\) | **2, 2** | 同 |
| \(d_u,d_v\) | **10, 4** | 同 |
| \(\lvert ek\rvert=\lvert ek_{\mathrm{PKE}}\rvert=\lvert ek_{\mathrm{KEM}}\rvert\) | **1184 B** | `OQS_KEM_ml_kem_768_length_public_key` |
| \(\lvert dk_{\mathrm{PKE}}\rvert\) | **1152 B** | \(384k\) |
| \(\lvert dk_{\mathrm{KEM}}\rvert\) | **2400 B** | `…_length_secret_key`；展开 `dk_pke‖ek‖H(ek)‖z` |
| \(\lvert c\rvert\) | **1088 B** | `…_length_ciphertext` |
| \(\lvert K\rvert,\lvert m\rvert,\lvert \rho\rvert,\lvert z\rvert,h\) | **32 B** | shared_secret / FIPS |
| keypair seed（liboqs derand） | **64 B**（\(d‖z\)） | `…_length_keypair_seed` |
| encaps seed（liboqs） | **32 B** | `…_length_encaps_seed` |

脚本自检：`bash scripts/check_mlkem768_sizes.sh`（对照 `thirdparty/liboqs/.../kem_ml_kem.h`）。

**展开恒等式（锁定）**：

```text
ek_kem == ek_pke                         # 1184
dk_pke = ByteEncode_12(ŝ)                # 1152 = 12*k*n/8
dk_kem = dk_pke ‖ ek_kem ‖ H(ek) ‖ z     # 1152+1184+32+32 = 2400
c = c1 ‖ c2                              # du*k*n/8 + dv*n/8 = 960+128 = 1088
```

---

## 2. 生产 I/O 契约（黑盒；P2/P3 实现须遵守）

### 2.1 PKE

| 算子 | 输入 | 输出 |
|------|------|------|
| Alg.13 KeyGen | `seed_d.bin`（+ LUT 若需） | `ek_pke.bin` 1184 · `dk_pke.bin` 1152 |
| Alg.14 Encrypt | `ek_pke` · `m` · `coins`（或设备内派生策略见 customspec） | `c.bin` 1088 |
| Alg.15 Decrypt | `dk_pke` · `c` | `m.bin` 32 |

中间 \(\hat A/\hat t/\hat y/u/v\) 等 **禁止**作为交付 I/O 落盘（调试 dump 标非默认）。

### 2.2 KEM

| 算子 | 输入 | 输出 |
|------|------|------|
| Alg.19 KeyGen | `seed_d`（+LUT） | `ek_kem` 1184 · `dk_kem` 2400 |
| Alg.20 Encaps | `ek_kem` · `m` | `c` 1088 · `K` 32 |
| Alg.21 Decaps（合法） | `dk_kem` · `c` | `K` 32 |
| Alg.21 Decaps（拒绝 / CT） | `dk_kem` · 假/`C_SRC` 密文 | `K`（\(J(z‖c)\)）≠ 合法路径 |

---

## 3. Tiling / 分核（T-B 锁定摘要）

> 细粒度 `SetSingleShape` / `blockDim` / `usedCoreNum` **尚未**数值锁定；P2 开写前在各 `INTEGRATION_PLAN` / customspec 补表。本卡只锁**语义不变量**。

| 不变量 | 锁定内容 |
|--------|----------|
| 噪声向量 | **polyvec6**：\(s‖e\) 共 6 poly；**每个 AIV 握完整 poly 的 hi+lo**（禁 limbsplit） |
| 矩阵 \(\hat A\) | \(3\times3\)=9 poly；**独立 prep launch**（不与 polyvec6 NTT 混为「pad 到 8」） |
| NTT S1–S3 | 禁 `Gather`（同 Tag5T 范围）；平面 mat_c；几何按 \(k=3\) **重推** |
| 禁止 | 末 poly 置零凑 4/8；复用 k4 的 8 路常量当默认 |

**待 P2 首刀前补齐的数值字段**（占位，未锁）：

| 字段 | 状态 |
|------|------|
| Stage2 MMAD `M,N,K` / `SetSingleShape` | 未锁 → W1 分析后填 |
| `blockDim` / `usedCoreNum` | 未锁 |
| AIV 负载划分（6 poly → 核映射） | 未锁（须服从「整 poly 同 AIV」） |

遇阻 **禁止**改参硬闯；停并重开讨论。

---

## 4. Derand / 域分离（草案锁定）

与 1024 的 `…-k4:…` **不得混用**（避免同 seed 跨参数组串味）。

| 用途 | 串（锁定草案） |
|------|----------------|
| PKE/KEM `d` from `SEED_D` | `exp-mlkem-f203-2s1e-k3:SEED_D=` ‖ 十进制 |
| KEM `z` from `SEED_D` | `exp-mlkem-f203-kem-k3:SEED_Z=` ‖ 十进制 |

若 P2 实现发现须与某 oracle 对齐而改串：须先改本卡并刷新 registry，不得静默改代码。

---

## 5. CT / liboqs 对照（P0-B）

| 项 | 值 |
|----|-----|
| 算法名 | `ML-KEM-768` / `OQS_KEM_alg_ml_kem_768` |
| 头文件 | `thirdparty/liboqs/src/kem/ml_kem/kem_ml_kem.h` |
| API | `OQS_KEM_ml_kem_768_{keypair,keypair_derand,encaps,encaps_derand,decaps}` |
| golden 角色 | **仅** I/O oracle；禁止逐步移植进 AscendC |
| 本仓 ref 胶水 | 预期扩展 `liboqs_kem_ref`（或 768 专用入口）；登记表未覆盖前 **停** |

### 5.1 长度交叉（已跑通）

| 宏 | 期望 | 实测（本机 thirdparty） |
|----|------|-------------------------|
| `…_public_key` | 1184 | 1184 |
| `…_secret_key` | 2400 | 2400 |
| `…_ciphertext` | 1088 | 1088 |
| `…_shared_secret` | 32 | 32 |

---

## 6. 目录落点（P0-D）

| 树 | 路径 | 说明 |
|----|------|------|
| 探针 | `ascendc-tests/ml-kem/ml-kem-768/` | 壳已建；见该树 `INDEX.md` |
| incubating | `examples/incubating/ml-kem/ml-kem-768/` | 壳已建；**写码前须 customspec** |
| stable | — | **不建** |

命名：探针 `pass-fix-f203-…-k3`；exp `exp-fips203-…-k3`。

---

## 7. Registry 草稿

六份骨架见 `docs/specs/fips203-mlkem768-*-baseline-registry.md`（计算块多为 **未验证**，P2 补绿后方可当交付依据）。

---

## 8. P0 退出清单

- [x] 用户决议写入本卡  
- [x] 长度与 liboqs 宏对拍  
- [x] T-B / C-1 / `-k3` / CT / PKE exp 锁定  
- [x] 目录壳 + INDEX  
- [x] registry 骨架  
- [x] P1 补洞与必建表（另文）  
- [ ] **P2 开写前**：补齐 §3 数值 tiling 表（按波次）
