# FIPS 203 ML-KEM-512 参数卡（P0 已锁）

**状态**：**已锁定**（2026-07-27 用户确认 §0）· **未实现**  
**参数组**：ML-KEM-512（k=2）  
**范围**：§0 已锁；本阶段 **不建** stable-512  
**完整计划**：[docs/research/MLKEM-512-从0到exp完整实现计划.md](../research/MLKEM-512-从0到exp完整实现计划.md)  
**P1 用例表**：[`fips203-mlkem512-p1-gap-and-cases.md`](fips203-mlkem512-p1-gap-and-cases.md)（**已定稿**）  
**对照**：[`fips203-mlkem768-parameter-card.md`](fips203-mlkem768-parameter-card.md)

---

## 0. 用户决议（2026-07-27 已确认）

> 与 [计划 §9](../research/MLKEM-512-从0到exp完整实现计划.md) 同步；用户已全部打钩。

- [x] **#1 单 AI Core** — **S-1**：单 cube（MIX `blockDim=1`）；cube 内可用 2 AIV；禁跨 cube 切 poly
- [x] **#2 噪声批** — **T-B2**：polyvec4（s‖e）；\hat A（2\times2）独立 prep；**禁零垫**
- [x] **#3 KEM device** — **保留** D19–D21
- [x] **#4 reject / CT** — **要求** device-ct + incubating-ct
- [x] **#5 PKE exp** — **要做**（三个 `exp-…-pke-*-k2`）
- [x] **#6 命名后缀** — `-k2`
- [x] **#7 liboqs-512 交叉** — **本阶段必达**
- [x] **#8 stable-512** — **本阶段不建**
- [x] **#9 自主推进** — 锁本卡后 Agent 按计划推进至有条件完成（停问点见计划 §0.1）

附加默认（随 #1–#9 一并锁定）：


| 项                   | 锁定草案                                  |
| ------------------- | ------------------------------------- |
| Compress/ByteEncode | **C-1**：512 树自建，默认 d\in10,4（密钥域 d=12） |
| Launch              | CPU 可多分段；SIM/生产少 launch；首版正确性优先       |
| NTT S1–S3           | 禁 `Gather`；禁 limbsplit；平面 mat_c       |


---



## 1. FIPS / liboqs 标量参数（已与头文件对拍）


| 符号                                   | 值          | 核对                                     |
| ------------------------------------ | ---------- | -------------------------------------- |
| n                                    | 256        | FIPS 203                               |
| q                                    | 3329       | FIPS 203                               |
| k                                    | **2**      | FIPS 203 Table 2                       |
| \eta_1,\eta_2                        | **3, 2**   | 同                                      |
| d_u,d_v                              | **10, 4**  | 同                                      |
| \lvert ek\rvert                      | **800 B**  | `OQS_KEM_ml_kem_512_length_public_key` |
| \lvert dk_{\mathrm{PKE}}\rvert       | **768 B**  | 384k                                   |
| \lvert dk_{\mathrm{KEM}}\rvert       | **1632 B** | `…_length_secret_key`                  |
| \lvert c\rvert                       | **768 B**  | `…_length_ciphertext`                  |
| \lvert K\rvert,\lvert m\rvert,\ldots | **32 B**   | shared_secret / FIPS                   |
| keypair seed                         | **64 B**   | `…_length_keypair_seed`                |
| encaps seed                          | **32 B**   | `…_length_encaps_seed`                 |


脚本（P0 待建）：`bash scripts/check_mlkem512_sizes.sh`。

```text
ek_kem == ek_pke                         # 800
dk_pke = ByteEncode_12(ŝ)                # 768
dk_kem = dk_pke ‖ ek_kem ‖ H(ek) ‖ z     # 1632
c = c1 ‖ c2                              # 640+128 = 768
```

---



## 2. 生产 I/O 契约（黑盒；实现须遵守）



### 2.1 PKE


| 算子             | 输入                        | 输出                          |
| -------------- | ------------------------- | --------------------------- |
| Alg.13 KeyGen  | `seed_d`（+LUT 若需）         | `ek_pke` 800 · `dk_pke` 768 |
| Alg.14 Encrypt | `ek_pke` · `m` · coins/派生 | `c` 768                     |
| Alg.15 Decrypt | `dk_pke` · `c`            | `m` 32                      |




### 2.2 KEM


| 算子                     | 输入             | 输出                           |
| ---------------------- | -------------- | ---------------------------- |
| Alg.19 KeyGen          | `seed_d`       | `ek_kem` 800 · `dk_kem` 1632 |
| Alg.20 Encaps          | `ek_kem` · `m` | `c` 768 · `K` 32             |
| Alg.21 Decaps（合法）      | `dk_kem` · `c` | `K` 32                       |
| Alg.21 Decaps（拒绝 / CT） | `dk_kem` · 假密文 | K=J(z‖c) ≠ accept            |


---



## 3. Tiling / 单核（草案；数值细节 W1 前再锁入卡）


| 不变量    | 草案                                           |
| ------ | -------------------------------------------- |
| 部署     | **S-1** 单 cube；`blockDim=1`（MIX 生产路径）        |
| 噪声     | **polyvec4**；每 AIV 握完整 poly hi+lo            |
| \hat A | 2\times2=4 poly；独立 prep                      |
| 禁止     | pad 到 3/4/6/8；复用 k3/k4 分核常数当默认；跨 cube 切 poly |


> W1 开写前须把 Inner/NTT AIV 负载、prep 分片、INTT batch、Decaps session 默认值写入本节（仿 768 §3.1–§3.3）。遇阻 **禁止**改参硬闯。

---



## 4. 目录落点（P0-D，待建）


| 树          | 路径                                       | P   | W     | 说明        |
| ---------- | ---------------------------------------- | --- | ----- | --------- |
| 探针         | `ascendc-tests/ml-kem/ml-kem-512/`       | P2  | W0–W3 | 待建        |
| incubating | `examples/incubating/ml-kem/ml-kem-512/` | P3  | W4    | 待建        |
| stable     | —                                        | —   | —     | **本阶段不建** |


命名：`pass-fix-f203-…-k2` / `exp-fips203-…-k2`。

---



## 5. Registry

六份 `docs/specs/fips203-mlkem512-*-baseline-registry.md`：P0 骨架 → incubating 绿后补登记。CBD‑\eta=3 计算块若登记表无来源 → **停**。

---



## 6. P0–P3 退出清单

- [x] 用户决议写入本卡（§0）
- [x] 长度与 liboqs 宏对拍脚本（`scripts/check_mlkem512_sizes.sh`）
- [x] S-1 / T-B2 / `-k2` / CT / PKE exp / liboqs 交叉锁定
- [x] 目录壳 + INDEX
- [x] registry 骨架
- [x] P1 缺项对照（补缺图）与必建表
- [ ] P2、W0–W3 探针全绿
- [ ] P3、W4 incubating 全绿
- [ ] P3、glue：AscendC RT + liboqs-512 KAT
- [ ] stable-512（须 `#交付#`，非本阶段）