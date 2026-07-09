# INTEGRATION_PLAN — fix-f203-alg14-pke-encrypt-correctness-k4

**定位**：`ascendc-tests/` **Alg.14 设备全链拼装探针**——用**已验收 AscendC 积木**多 launch 拼出 `c ← Encrypt(ek, m)`；**非** `examples/` 交付、**非** liboqs / 外部 KEM 黑盒走生产路径。

**自包含约束**：[`SELF_CONTAINED.md`](SELF_CONTAINED.md)（对齐 KeyGen pass 探针教训：外部 oracle 不得渗入默认 `run.sh`）。

**参照 KeyGen 终态**：[`pass-fix-f203-alg13-device-keygen-k4`](../pass-fix-f203-alg13-device-keygen-k4/) · [`stable-fips203-mlkem-pke-keygen-k4`](../../examples/stable/stable-fips203-mlkem-pke-keygen-k4/)

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
| **L8** `Compress_d` + `ByteEncode_d` | 行 22–23 | [`pass-f203-compress-d-vec-k4`](../pass-f203-compress-d-vec-k4/) + [`pass-f203-byteencode-d-vec-k4`](../pass-f203-byteencode-d-vec-k4/) | **Compress/ByteEncode d=4/5/10/11** PASS |

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
| `t̂` polyvec | 4×384 B | vendored [`pass-f203-alg6-bytedecode-d-vec-k4`](../pass-f203-alg6-bytedecode-d-vec-k4/) 的 **d=12** 路径（与 KeyGen ByteEncode₁₂ 互逆） |
| `ρ` | 32 B | `ek_pke[1536:1568]`，供 L1 SampleNTT |

### 2.3 `at_r5` G3 合并核 — 数学约定（2026-06-30）

**背景**：本地已证实 [`func_key ≥ 5 → 507000`](../../qa/2026-06/2026-06-30-funckey-507000本地独立验证.md) 病根。SIM `device_aiv.o` AIV-only 核须压到 **5 个**，G3 必须收进**单次 launch**（旧 `g3_linear`/`g3_linear4`/`at_r×2 多 session`/`t_dot_r` 均无解：拆核失败、多 session 失败、func_key 越界）。

**核心思路**：把现有 `at_r`（k=4 输入、`kPOut=4` 输出，`out[p] = Σ_j Â[j,p] *_NTT r̂[j]` = `û[p]`）扩到 `kPOut=5`，第 5 列把 `t̂[j]` 当作 Â 的「虚拟列 4」即得 `tr̂ = Σ_j t̂[j] *_NTT r̂[j]`。

**Host matM 拼装**（`[k=4, kP=5, N=256] int32` 行主序，索引 `(j*kP + p)*N + n`）：

| j | p | matM[j,p,·] 来源 |
|---|----|-------------------|
| 0..3 | 0..3 | GM aHat 对应位置：`aHat[(j*K + p)*N]`（即 `Â[j, p]`，与 `a_hat_offset(j, p)` 一致）|
| 0..3 | 4 | GM tHat 对应位置：`tHat[j*N]`（即 `t̂[j]`） |

**Host 流程**（必须按序）：

```
prep_a_hat → aHatDev    （AIV_ONLY launch）
decode_t_hat → tHatDev  （AIV_ONLY 或 MIX_AIC_1_2 占位 launch）
aclrtSynchronizeStream(stream)              ← 关键：D2H 前必同步（病根 2）
D2H aHatDev → aHatHost
D2H tHatDev → tHatHost
host 拼 matHost[(j*5+p)*N+n] = (p<4) ? aHatHost[(j*4+p)*N+n] : tHatHost[j*N+n]
H2D matHost → matDev
ACLRT_LAUNCH_KERNEL(f203_encrypt_at_r5)(blockDim, stream, matDev, rHatDev, uTrDev)
```

**Device 算法**（与现有 `at_r` 同 innerproduct UB 流水，仅 `kPOut`/`kSVec` 维度更换）：

```
for p_out ∈ [0..kP-1]:                        // kP=5
    acc[p_out] = 0
    for j ∈ [0..k-1]:                         // k=4
        f = matM[(j*kP + p_out)*N : ...]      // = matM[j, p_out]
        g = rHat[j*N : ...]                   // = r̂[j]
        acc[p_out] += multiply_ntts(f, g)
    uTr[p_out*N : ...] = barrett_mod_q(acc[p_out])
```

**输出 uTrDev `[kP=5, N] int32` 行主序**：

| p | 内容 | 数学等价 |
|---|------|----------|
| 0..3 | `uTr[p] = Σ_j Â[j, p] *_NTT r̂[j]` | `û[p]` = `(Âᵀ·r̂)[p]`（FIPS 203 Alg.14 §18） |
| 4 | `uTr[4] = Σ_j t̂[j] *_NTT r̂[j]` | `tr̂` = `(t̂·r̂)`（FIPS 203 Alg.14 §19 前置） |

**Host D2H 拆分**：`uHatDev = uTrDev[0 : 4*N*4 B]`、`trHatDev = uTrDev[4*N*4 B : 5*N*4 B]`（拼接式连续布局；无需独立 GM 块）。

**与旧路径的关系**：

| 旧 | 新 |
|---|---|
| 4 个独立 G3 核（`g3_linear`/`g3_linear4`/`at_r`/`t_dot_r`）+ 多 session 绕行 | 1 个 `at_r5` + 单 session |
| SIM AIV-only 占用 4 个 key | SIM AIV-only 占用 1 个 key（释放配额给 `g4_noise`/`pack` 等） |
| `tr̂` 路径需 `pack_t_hat_as_at_r_col0` workaround 或 `t_dot_r` | 单核内一次出 `tr̂` |

CPU `#ifdef ASCENDC_CPU_DEBUG` 走 host scalar 版（与 device 同公式），用于孪生对拍；SIM/NPU 走向量 UB 实现。旧 4 个 G3 核在 SIM/NPU 端从 `KERNEL_FILES` 移除（CPU build 保留以确保历史 ICPU_RUN_KF 兼容性，或同步删除）。

---

### 2.4 `d_u=11` / `d_v=5`（2026-07-01 对齐 liboqs）

[`pass-f203-compress-d-vec-k4`](../pass-f203-compress-d-vec-k4/IMPLEMENTATION_PLAN.md) 已列 Barrett 常数（**d=4/5/10/11 PASS**）；[`pass-f203-byteencode-d-vec-k4`](../pass-f203-byteencode-d-vec-k4/) 为参数化 pack（**d=4/5/10/11**）。

**`Compress_5` 定点契约**（pack 路径，与 liboqs ref 一致）：

```text
d0 = u * 1290176
out = (d0 + (1 << 26)) >> 27    // 再 & 0x1F；勿用 (1<<27)
```

`Compress_11` 仍为 `(d0 + (1<<32)) >> 33`。详 [`docs/notes/F203-PKE-liboqs交叉验证与Compress定点技术总结.md`](../../docs/notes/F203-PKE-liboqs交叉验证与Compress定点技术总结.md) §2。

**ek 输入**（生产已有 `ek_pke.bin`）：

| 块 | 尺寸 | 来源 |
|----|------|------|
| `t̂` polyvec | 4×384 B | §2.2 ByteDecode₁₂ |
| `ρ` | 32 B | `ek[1536:1568]` |

---

## 3. 分阶段 Gate（过渡 → 生产）

**治理规则**（2026-06-30）：**G5** = 唯一生产验收；**G0–G4** = 过渡路线，**每测通下一 Gate 即冻结上一 Gate**。G5 双模式 PASS 后 G0–G4 **全部关闭**。详 [`frozen-gates/FROZEN.md`](frozen-gates/FROZEN.md)。

| Gate | 设备路径 | 状态 | 验收（历史） |
|------|----------|------|--------------|
| **G0** launch 壳 | marker AIV×1 | **已冻结** | kernel 正常结束 |
| **G1** prep | L1 + L2（2 launch） | **已冻结** | 中间 GM vs golden |
| **G2** NTT 域 | L3 on `r̂` | **已冻结** | `r̂` vs golden |
| **G3** 线性层 | L4–L6 | **已冻结** | `û`/`tr̂` vs golden；旧四核 → `compute/frozen/` |
| **G4** 落盘 | L7 + L8（含 Host scalar tail） | **已冻结** | `c.bin` max=0（G5 前 SIM 绕行） |
| **G5** 生产 I/O | 单 session 全 device | **活跃** | gate + `c.bin` max=0；`input/` 仅 ek/m/coins |

**当前**：**G5 双模式 PASS — SIM 已测通**（2026-06-30）。标准验收：`bash run.sh -r cpu/sim -v Ascend910B4`（默认 G5，勿写 `ENCRYPT_GATE`）。

> SIM 是否测通：**通过**。命令 `ENCRYPT_VERIFY=1 bash run.sh -r sim -v Ascend910B4`，退出码 0，关键日志 `[verify_gate] G3 u_hat + tr_hat PASS` / `[verify] PASS max=0 (1568 bytes)` / `[SUCCESS] gate=G5 (sim) ENCRYPT_VERIFY=1`，全程无 `507000`。详 [`STATUS.md`](STATUS.md) §SIM 测试通过声明。

| 验证条目 | CPU | SIM |
|---|---|---|
| `bash run.sh -r <mode> -v Ascend910B4` G5 gate (`ENCRYPT_VERIFY=0`) | ✅ `gate_g1/g2/g3` 全 PASS、`c.bin` 1568B 写出 | ✅ 同左，**无 507000** |
| `ENCRYPT_VERIFY=1` (`c.bin` vs `golden_c.bin`) | ✅ `[verify] PASS max=0 (1568 bytes)` | ✅ `[verify] PASS max=0 (1568 bytes)` |
| host binary 仅引用 `aclrtlaunch_f203_encrypt_at_r5`（`nm` 无 `g3_linear/at_r/t_dot_r`） | — | ✅ |
| `out/include/ascendc_kernels_sim/aclrtlaunch_*.h` 列表 = 11 个活跃核（无 g3_linear/at_r/t_dot_r） | — | ✅ |
| SIM `Total tick`（CAModel） | n/a | **43479** |

旧 4 核已迁入 [`compute/frozen/`](compute/frozen/)（`frozen-g3_linear` / `frozen-at_r` / `frozen-t_dot_r`），**禁止**加回 `KERNEL_FILES`。`F203_FUNCKEY_EXPERIMENT=0` 仍是生产默认。

---

## 4. Launch 编排（终态：单 ACL session，2026-06-30 重做）

**SIM device_aiv.o AIV-only 核**（**严格 ≤ 5**，按 nm func_key 验证）：

| key | kernel | KERNEL_TASK_TYPE | 备注 |
|-----|--------|------------------|------|
| 0 | `f203_encrypt_marker_custom` | AIV_ONLY | launch 壳健康检查 |
| 1 | `f203_encrypt_prep_a_hat` | AIV_ONLY | ρ → 16 poly Â（含 SHAKE + Alg.7） |
| 2 | `f203_encrypt_prep_re` | AIV_ONLY | coins → r/e₁/e₂（CBD η₁/η₂） |
| 3 | `f203_encrypt_g4_noise` | AIV_ONLY | u_time/tr_time + e₁/e₂ + μ → uOut + vOut |
| 4 | `f203_encrypt_at_r5` | AIV_ONLY | **新 G3 合并核**（§2.3） |

**MIX 核 / MIX 占位**（SIM/NPU 走 MIX_AIC_1_2；CPU 仍 AIV_ONLY）：

| kernel | 角色 |
|--------|------|
| `f203_encrypt_ntt_r` | MIX（已有） |
| `f203_encrypt_intt` | MIX（已有） |
| `f203_encrypt_decode_t_hat` | **MIX 占位**（ek → t̂；占位绕 §C4 风险） |
| `f203_encrypt_pack` | **MIX 占位**（Compress₁₁/₅ + ByteEncode → c） |

> **若 P2 验证 `g4_noise` 走 AIV_ONLY (key=3) PASS**：完成；否则将 `g4_noise` 也改 MIX 占位，释出的 key 由其它需要 launch 的核占。

**单 session 编排**：

```text
aclInit / aclrtSetDevice / aclrtCreateStream                                ← 1 次
  │
  ├─ marker_custom                                       （壳）
  ├─ prep_a_hat   → aHatDev          [4*4*256 i32]
  ├─ prep_re      → reDev            [4+4+1 polys]      （含 r/e₁/e₂）
  ├─ ntt_r        → rHatDev          [4*256 i32]
  ├─ decode_t_hat → tHatDev          [4*256 i32]        （MIX 占位）
  ├─ aclrtSynchronizeStream(stream)                      ← §2.3 病根 2
  ├─ host D2H aHat/tHat → 拼 matM[(j*5+p)*N+n] → H2D matDev
  ├─ at_r5(matDev, rHatDev, uTrDev)  → uTrDev[5*256 i32]
  ├─ INTT(uTrDev[0..3*256])   → uTimeDev   [4*256 i32]   (MIX)
  ├─ INTT(uTrDev[4*256..5*256])→ trTimeDev [256 i32]     (MIX；polyvec INTT 单 poly tail)
  ├─ g4_noise(uTimeDev, e₁Dev, trTimeDev, e₂Dev, mDev, vDev)
  │       → uTimeDev += e₁、vDev = NTT⁻¹(trHat)+e₂+Decompress₁(m)
  ├─ pack(uTimeDev, vDev, cDev)                          （MIX 占位）
  └─ D2H cDev → c.bin
aclrtSynchronizeStream → DestroyStream → ResetDevice → aclFinalize           ← 1 次
```

**关键守则**（执行前必核）：

1. `nm build/ascendc_kernels_sim_aiv_device_dir/device_aiv.o | grep "^[0-9a-f]\+ T f203"` 必显示**恰好 5 行**，且**要 launch 的核 func_key ≤ 4**。
2. 每次 host↔device 往返打包前 `aclrtSynchronizeStream(stream)`。
3. 全链**只**一次 `aclInit/aclFinalize`，避免 `free(): invalid pointer`。
4. `g4_noise` 改为「直接消费 INTT 时域 + e/μ」六参 launch，不再走 host 标量；`pack` 同样 device 化。
5. 旧 G3 4 核（`g3_linear`/`g3_linear4`/`at_r`/`t_dot_r`）从 SIM/NPU `KERNEL_FILES` 移除；CPU 保留 `at_r5` host scalar 走孪生路径。
6. `F203_FUNCKEY_EXPERIMENT` 实验开关保留默认 OFF，仅作[病根证据](../../qa/2026-06/2026-06-30-funckey-507000本地独立验证.md)；不与新 G3 路径相互依赖。

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
| **定型交付** | 另建 `examples/incubating/exp-fips203-mlkem-pke-encrypt-k4`（须 customspec） |

**当前工程顺序**：G1 prep vendored → G2 NTT → G3 线性层（含 Âᵀ 索引）→ G4 pack → G5 合并 launch。

---

## 8. 与 KeyGen 的差异（易混）

| | KeyGen Alg.13 | Encrypt Alg.14 |
|--|---------------|----------------|
| 噪声 | η, η, η (s,e) | η₁ (r,e₁), η₂ (e₂) |
| 矩阵乘 | `Â·ŝ`：读 `A[p,j]` | `Âᵀ·r̂`：读 `A[j,p]`（**同 GM**，索引对调） |
| 输出 pack | ByteEncode₁₂ → ek | Compress+ByteEncode **d_u/d_v** → c |
| 已有全链 | pass keygen ✅ | **at_r5 落地 PASS ✅**（2026-06-30，详 §9）|

---

## 9. 排查史与 PASS 路径回顾（2026-06-30）

> 完整原理沉淀：[`docs/notes/AscendC-CAModel-SIM-funckey与单session约束知识库.md`](../../docs/notes/AscendC-CAModel-SIM-funckey与单session约束知识库.md)（**先读它**，本节只是本探针局部时间线）。  
> 讨论纪要：[`qa/2026-06/2026-06-30-funckey-507000本地独立验证.md`](../../qa/2026-06/2026-06-30-funckey-507000本地独立验证.md)（§9 排查回顾、§10 性能、§11 经验）。  
> 历史错误判读原文：[`G3_SIM_AUDIT.md`](G3_SIM_AUDIT.md) §1–§11，**已被 §12 修正**。

### 9.1 真正病根

| # | 病根 | 触发面 |
|---|------|--------|
| R1 | CAModel SIM 单 binary 内 **AIV-only kernel `func_key ≥ 5`** 一律 `aclrtLaunchKernel` 返回 `507000` | 编译期（`KERNEL_FILES` 集合 + ascendc 的 `func_key` 分配） |
| R2 | host D2H 前未 `aclrtSynchronizeStream`，或一次推理内多次 `aclInit/aclFinalize` | 运行期（host 编排） |

### 9.2 错误尝试 ↔ 真因映射

| 当时（2026-06-19 ~ 06-29）的判读 | 实际是 R1/R2 哪一面 |
|-------------------------------|--------------------|
| 「`f203_encrypt_t_dot_r` 入口 SIM 注册失效」→ 用 `at_r(t̂_col0)` 等价绕开 | `t_dot_r` `func_key=7`，踩 R1 |
| 「`f203_encrypt_g3_linear` 五参 ABI SIM 不兼容」→ 退化为两次 `at_r` 独立 session | `g3_linear` `func_key=5`，踩 R1；多 session 又踩 R2 |
| 「SIM 上必须多段 `aclInit/aclFinalize`」 | 反了；多 session 反而是 R2 触发面 |
| 「`g4_noise` / `pack` SIM 不支持，必须 host scalar」 | `g4_noise` `func_key=8`、`pack` `func_key=11`，全部踩 R1（这两个核没坏）|
| 「SIM 末尾 `free(): invalid pointer` 疑 ACL 多段副作用」 | 单 session 后此现象消失，本质就是 R2 |

### 9.3 正确路径（at_r5 + 单 session）

1. 家里 agent 在远端 `27cc93b` 用对照实验提出 R1 命题；本地拒 pull，改在原探针做正向证伪。
2. 加 `F203_FUNCKEY_EXPERIMENT` CMake 开关：ON 时移除 4 个文件，`device_aiv.o` 缩到 5 个 AIV-only 核。
3. 同一份 `f203_encrypt_g4_noise` kernel：默认 build `func_key=8` → 507000；实验 build `func_key=4` → **ret=0**。R1 坐实。
4. 结构性重做 G3：`at_r5` 合并核（kP=5）单 launch 出 `[û \| tr̂]`；host 拼 `matM`；prep + NTT + decode + at_r5 单 session。
5. `aclrtSynchronizeStream` 在所有 host 读 device GM 之前显式放置（R2 闭合）。
6. 从 `KERNEL_FILES` 永久剔除 `compute/g3_linear/f203_encrypt_g3_linear.cpp`（连带 `g3_linear` / `g3_linear4` / `at_r` / `t_dot_r` 4 个旧 kernel）；源文件保留为历史证据。
7. `F203_FUNCKEY_EXPERIMENT` 守卫永久保留（默认 OFF），保证「R1 边界可重现实验」始终可重跑。

### 9.4 验收与性能

| 命令 | 结果 |
|------|------|
| `bash run.sh -r cpu -v Ascend910B4` | `gate_g1/g2/g3` PASS、c.bin 1568B |
| `ENCRYPT_VERIFY=1 bash run.sh -r cpu -v Ascend910B4` | `[verify] PASS max=0 (1568 bytes)` |
| `bash run.sh -r sim -v Ascend910B4` | `gate_g1/g2/g3` PASS、**无 507000** |
| `ENCRYPT_VERIFY=1 bash run.sh -r sim -v Ascend910B4` | `[verify] PASS max=0 (1568 bytes)` |
| SIM `Total tick`（CAModel）| **43479**（旧两次 `at_r` ≈ 87600，~50% 节省，**结构性**而非 vector 加速）|
| SIM `wall_sec`（含 build + verify） | ~334s |

### 9.5 经验提炼（要在后续探针**第一时间**想到）

| L | 内容 |
|---|------|
| L1 | SIM `507000` 第一反应：`nm device_aiv.o` 看 `func_key`，再看是不是单 session；算法/入口/ABI 留到最后查 |
| L2 | AIV-only 核数 ≤ 5 是**设计期资源**；开新算子前先列「目标核清单 + `func_key` 名额预算」 |
| L3 | 能合并就合并（多输出单 launch）；剩余复杂度靠 host 拼装吸收 |
| L4 | `aclInit / aclFinalize` 在一次推理内只一次；任何「子函数里另开 session」都是隐患 |
| L5 | `aclrtMemcpy(... DEVICE_TO_HOST)` 前显式 `aclrtSynchronizeStream`，不靠 launch 返回 = 完成的错觉 |
| L6 | 受控实验代码（`F203_FUNCKEY_EXPERIMENT`）默认 OFF + 永久保留，比文档管用十倍 |
| L7 | CPU PASS **不能**代表 SIM PASS；CPU 不暴露 R1/R2 |
| L8 | 性能比较只用 `Total tick`，不用 `wall_sec` |

G0–G4 过渡路线已标准化冻结 → [`frozen-gates/FROZEN.md`](frozen-gates/FROZEN.md)（含 `g4_add_e1`/`g4_make_v` 拆分核）。

---

## 10. 仓库级 liboqs 交叉验证（2026-07-01）

**边界**：本探针 [`SELF_CONTAINED.md`](SELF_CONTAINED.md) **禁止** liboqs 进入默认 `run.sh` / 设备核。与 liboqs 的字节对拍由 **仓库根** 脚本承担，作为 L1（host golden）之外的 **L2 外部 oracle**。

| 脚本 | 作用 |
|------|------|
| [`scripts/liboqs_pke_vs_ascendc.sh`](../../scripts/liboqs_pke_vs_ascendc.sh) | KeyGen / Encrypt / Decrypt 三阶段 vs liboqs |
| [`scripts/build_liboqs_pke_ref.sh`](../../scripts/build_liboqs_pke_ref.sh) | 编译 `liboqs_pke_ref`（依赖 `thirdparty/liboqs`） |

**Encrypt 段 I/O**：AscendC **ek**（本探针或 KeyGen 产出）+ liboqs fixture 的 **m/coins** → 对拍 **c.bin**（1568B）。

**2026-07-01 修复**：`pack/` 与 `scripts/host_golden/` 中 **`Compress_5` 舍入偏置** 由 `(1<<27)` 改为 `(1<<26)`（与 liboqs `mlk_scalar_compress_d5` 一致）。修前 c₁（d=11）已对、仅 c₂ 错；修后 L2 **CPU+SIM max=0**（`SEED_D=20260619`）。

详 [`docs/notes/F203-PKE-liboqs交叉验证与Compress定点技术总结.md`](../../docs/notes/F203-PKE-liboqs交叉验证与Compress定点技术总结.md) · 纪要 [`qa/2026-07/2026-07-01-liboqs验证与KEM-Alg19-KeyGen规划.md`](../../qa/2026-07/2026-07-01-liboqs验证与KEM-Alg19-KeyGen规划.md)。

```bash
bash scripts/liboqs_pke_vs_ascendc.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash scripts/liboqs_pke_vs_ascendc.sh -r sim -v Ascend910B4
```

**与 L3 round-trip 关系**：[`scripts/roundtrip_pke_encrypt_decrypt.sh`](../../scripts/roundtrip_pke_encrypt_decrypt.sh) 证明 device c→m 闭环；L2 证明各阶段与 liboqs 一致。三层互补，见 note §3。
