# FIPS 203 ML-KEM-768 参数卡（P0 锁定）

**状态**：**已锁定**（2026-07-26 用户确认 §决议）；**实现有条件完成至 incubating**（2026-07-26/27）
**参数组**：ML-KEM-768（\(k=3\)）
**范围**：P0 文书已锁；P2（W0–W3）+ P3（W4 + glue）已绿；**本阶段不建** stable-768
**完整计划**：[docs/research/MLKEM-768-从0到exp完整实现计划.md](../research/MLKEM-768-从0到exp完整实现计划.md)
**P1 用例表**：[fips203-mlkem768-p1-gap-and-cases.md](fips203-mlkem768-p1-gap-and-cases.md)
**收尾纪要**：[qa/2026-07/2026-07-27-768收尾复盘与文档刷新.md](../../qa/2026-07/2026-07-27-768收尾复盘与文档刷新.md)

---

## 0. 用户决议（2026-07-26）

| # | 议题 | 决议 |
|---|------|------|
| 1 | 分核主路线 | **T-B**：噪声侧 **polyvec6**（s‖e）；\(\hat A\)（\(3\times3\)）**独立 prep launch** |
| 2 | KEM device 探针 | **保留** D19–D21 |
| 3 | reject / CT | **要求** `…-decaps-device-ct-k3` 与 incubating `…-decaps-ct-k3` |
| 4 | PKE exp | **要做**（keygen / encrypt / decrypt 三个 `exp-…-pke-*-k3`） |
| 5 | 命名后缀 | **`-k3`** |
| 6 | 本轮范围（当时） | **先完成 P0 + P1**（文书 + 目录壳）；其后已授权并完成 P2（W0–W3）+ P3（W4 + glue） |

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

> **W1 数值 tiling 已锁**（2026-07-26，用户授权 P2、W1）。语义不变量仍优先；遇阻 **禁止**改参硬闯。

| 不变量 | 锁定内容 |
|--------|----------|
| 噪声向量 | **polyvec6**：\(s‖e\) 共 6 poly；**每个 AIV 握完整 poly 的 hi+lo**（禁 limbsplit） |
| 矩阵 \(\hat A\) | \(3\times3\)=9 poly；**独立 prep launch**（不与 polyvec6 NTT 混为「pad 到 8」） |
| NTT S1–S3 | 禁 `Gather`（同 Tag5T 范围）；平面 mat_c；几何按 \(k=3\) **重推** |
| 禁止 | 末 poly 置零凑 4/8；复用 k4 的 8 路常量当默认 |

### 3.1 W1 数值锁定（B4–B6）

| 探针 | 字段 | 锁定值 |
|------|------|--------|
| **B4** SampleNTT | 范围 | 单 poly `(j,i)`；验收矩阵 \((j,i)\in\{0,1,2\}^2\) |
| | `k` in `G(d‖byte(k))` | **3** |
| | derand 前缀 | `exp-mlkem-f203-2s1e-k3:SEED_D=` |
| | XOF / rej | 672B / 224 cand（与 k 无关） |
| | `blockDim` | **1**（AIV_ONLY） |
| **B5** Stage123 | `kK` / ROWS | **6**（polyvec6） |
| | S0 紧凑 | **`[HI₆,LO₆]=[12,256] int8`**（禁 pad 到 16 行假 poly） |
| | 平面 mat_c | **`[48,128] int32`**（`6×4×2`） |
| | Stage2 MMAD | 逻辑 **M=12**, K=256, N=128；硬件 `mmadParams.m=ceil(12/16)*16=16`（Cube 对齐垫，**非**假 poly） |
| | MIX / `blockDim` | **1** → 1 AIC + 2 AIV |
| | AIV poly 映射 | **连续** AIV0 `{0,1,2}` / AIV1 `{3,4,5}`（`kPolysPerAiv=3`） |
| | 与 CBD 分片关系 | B3 CBD 可用交叉 `{0,1,3}\|{2,4,5}`；NTT 批处理锁连续 3+3；融合时由 prep 重排，**不**为对齐 CBD 而改本表 |
| **B6** Multiply+Inner | Alg.11 | 单对 `(f,g)→h`；与 k 无关；`blockDim=1` |
| | InnerProduct | **`P_OUT=S_VEC=3`**；GM `a_hat[9,256]` / `s_hat[3,256]` / `t_hat[3,256]` |
| | Inner 默认分核 | **`blockDim=2`**：AIV0 `t̂[{0,1}]`、AIV1 `t̂[{2}]`（**禁止** `P/2` 整除假设） |

遇阻 **禁止**改参硬闯；停并重开讨论。

### 3.2 W2 数值锁定（D13–D15）

> **W2 数值 tiling 已锁**（2026-07-26，用户授权「记性能后继续」）。编排对齐活跃 k4 device（prep→compute），几何按 \(k=3\) **重推**；**禁止**零垫凑 4/8。

| 探针 | 字段 | 锁定值 |
|------|------|--------|
| **共用** | SIM/生产 launch | D13 **2**（prep→compute）；D14 **2**（prep→compute+tail）；D15 **1**（fused） |
| | CPU | 可多分段对照；**非**性能基线 |
| | I/O | 参数卡 §2：ek **1184** / dk_pke **1152** / c **1088** / m **32** |
| | \(d_u,d_v\) | **10, 4**；c1=960、c2=128 |
| | Derand | `exp-mlkem-f203-2s1e-k3:SEED_D=`（§4） |
| **D13** KeyGen | prep | AIV_ONLY **`blockDim=2`**：Â SampleNTT \(3\times3=9\)（分片 **5+4**）+ CBD \(\eta=2\) polyvec6（B3 交叉分片可先写 GM，compute 前按 B5 连续序消费） |
| | compute | MIX **`blockDim=1`**：polyvec6 NTT（B5）→ Inner \(P=3\)（B6，AIV **2+1**）→ ByteEncode₁₂ **3×384=1152** → `ek‖ρ` 融合 |
| | Â GM | **`[9,256] int32`**（禁 pad 到 16） |
| | `s‖e` | **`[6,256]`**；NTT 后 `ŝ` 前 3 行进 dk |
| **D14** Encrypt | prep | AIV_ONLY **`blockDim=2`**：从 ek 取 \(\rho\) 扩 Â（9）+ CBD `r‖e₁‖e₂`（**7** poly = \(2k+1\)） |
| | compute | MIX **`blockDim=1`**：NTT(\(r\)) \(k=3\) → Â∘r̂ / 点积 → **INTT batch=4**（\(\hat u\|v\)，**禁** pad 到 6/8）→ Compress+\(d_u{=}10,d_v{=}4\) ByteEncode → `c` |
| | INTT 几何 | 真 **polyvec4**：S0 **`[8,256] int8`**；mat_c 按 \(4\times4\times2\Rightarrow[32,128]\)；AIV **连续 2+2**；Cube M 对齐垫≠假 poly |
| **D15** Decrypt | fused | **`blockDim=1`**（与 k4 PASS 同档）：Decode₁₂(dk) → unpack \(c\)（d=10/4）→ NTT(\(u\)) \(k=3\) → 点积 → INTT → Compress₁+ByteEncode₁ → `m` |
| | 禁 | 复用 k4 的 `kInttBatch=8` / c 长度 1568 / ek 1568 常量当默认 |

积木复用（活跃 k3，非 frozen）：B1–B6 路径见 [`ml-kem-768/INDEX.md`](../../ascendc-tests/ml-kem/ml-kem-768/INDEX.md)。

遇阻 **禁止**改参硬闯；停并重开讨论。

### 3.3 W3 数值锁定（D19–D21[+ct]）

> **W3 数值 tiling 已锁**（2026-07-26，用户授权「我授权」开 W3）。KEM 设备探针在活跃 **D13–D15** 之上加 Alg.16/17/18 头尾；几何复用 §3.2；**禁止**零垫与 k4 字节长默认。

| 探针 | 字段 | 锁定值 |
|------|------|--------|
| **共用** | I/O | ek_kem **1184**＝ek_pke；dk_kem **2400**＝dk_pke(1152)‖ek(1184)‖H(ek)(32)‖z(32)；c **1088**；K/m **32** |
| | Derand | `d`：`exp-mlkem-f203-2s1e-k3:SEED_D=`；`z`：`exp-mlkem-f203-kem-k3:SEED_Z=`（§4） |
| | PKE 内核几何 | **原样复用** §3.2 D13/D14/D15（Â[9]、polyvec6、INTT batch4、d_u/d_v=10/4） |
| | 禁 | k4 的 ek=1568 / dk_kem=3168 / c=1568；pad 凑 4/8；从 `frozen/` 抄码 |
| **D19** Alg.19 KeyGen | Launch | SIM/生产 **2**：prep（＝D13 prep）→ compute+**Alg.16 尾**（H(ek)‖拼 dk_kem；内嵌，无第 3 launch） |
| | blockDim | prep **2** AIV_ONLY；compute MIX **1** |
| | 输出 | `ek_kem` 1184 · `dk_kem` 2400 |
| **D20** Alg.20 Encaps | Launch | SIM **2** / CPU 可多分段（对齐 D14）；头 `G(m‖H(ek))`→(K̄,r) 后走 D14 Encrypt |
| | 输出 | `c` 1088 · `K` 32 |
| | 验收 | `c`/`K` max=0 vs golden（或 liboqs-768 encaps_derand） |
| **D21** Alg.21 Decaps（交付） | 组成 | Phase-D＝D15 Decrypt → Phase-E＝D14 形重加密 + FO（`K` 或 `J(z‖c)`） |
| | Launch | SIM 默认对齐 k4 交付：**少 launch**（目标 3；CPU 可多段）；**默认 `decaps_1session`** |
| | 输出 | 合法路径 `K` 32；可选 `KEM_DECAPS_REJECT=1` 拒绝路径 |
| **D21ct** Alg.21 CT | 与 D21 | 同 I/O；生产 SIM 默认 **`decaps_2session`**（CT 锁定） |
| | 验收 | 合法 `K` max=0；拒绝路径 `REJECT PASS` 且 **reject≠accept** |

积木复用：D13–D15 + B1–B6（活跃 k3）。incubating/stable-768 **本波不写**。

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
| 本仓 ref 胶水 | incubating registry 已按绿线补登记；**liboqs-768 helper / 交叉 KAT** 仍属可选（T768-post）；当前 RT 为 AscendC-only |

### 5.1 长度交叉（已跑通）

| 宏 | 期望 | 实测（本机 thirdparty） |
|----|------|-------------------------|
| `…_public_key` | 1184 | 1184 |
| `…_secret_key` | 2400 | 2400 |
| `…_ciphertext` | 1088 | 1088 |
| `…_shared_secret` | 32 | 32 |

---

## 6. 目录落点（P0-D）

| 树 | 路径 | P | W | 说明 |
|----|------|---|---|------|
| 探针 | `ascendc-tests/ml-kem/ml-kem-768/` | P2 | W0–W3 | 全绿；见该树 `INDEX.md` |
| incubating | `examples/incubating/ml-kem/ml-kem-768/` | P3 | W4 | E13–E21ct 全绿（均有 customspec） |
| stable | — | — | — | **本阶段不建**（须 `#交付#`） |

命名：探针 `pass-fix-f203-…-k3`；exp `exp-fips203-…-k3`。

---

## 7. Registry

六份见 `docs/specs/fips203-mlkem768-*-baseline-registry.md`；**incubating 绿线已补登记**（2026-07-26）。晋级 stable 前仍须按 ascendc-delivery 再审；liboqs-768 交叉属 T768-post。

---

## 8. P0–P3 退出清单

- [x] 用户决议写入本卡
- [x] 长度与 liboqs 宏对拍
- [x] T-B / C-1 / `-k3` / CT / PKE exp 锁定
- [x] 目录壳 + INDEX
- [x] registry 骨架 → incubating 补绿
- [x] P1 缺项对照（补缺图）与必建表（另文）
- [x] P2、W0–W1：积木 B1–B6
- [x] P2、W2：§3.2 / D13–D15
- [x] P2、W3：§3.3 / D19–D21[+ct]
- [x] P3、W4：E13–E15、E19–E21ct
- [x] P3、glue：AscendC-only roundtrip
- [ ] stable-768（须 `#交付#`）
- [ ] liboqs-768 helper / device KAT（T768-post，可选）
