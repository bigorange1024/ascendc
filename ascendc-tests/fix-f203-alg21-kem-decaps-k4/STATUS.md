# STATUS — fix-f203-alg21-kem-decaps-k4

FIPS 203 **Algorithm 21 `ML-KEM.Decaps(dk, c)`**（**ml_kem_1024 / k=4**）。

| 项 | 值 |
|---|---|
| **阶段** | **单设备库 + SIM 默认 2-session**（2026-07-03）：CPU 单 session `K max=0` **PASS**；SIM 默认 `KEM_DECAPS_SIM_2SESSION=1` → `K max=0` **PASS** |
| **I/O** | `dk_kem` **3168B** · `c` **1568B** · `K` **32B** |
| **输入来源** | alg19 `dk_kem.bin` + alg20 `c.bin`（`SEED_D=20260619`） |

## 2026-07-02 单库合并（家里 agent · SIM 单 session 修复）

**根因修正**：SIM 单 session 重加密 `c' max=244` **不是**「泛化 CAModel 状态污染」，而是探针曾用 **decrypt/encrypt 双设备库**在一个 ACL session 内 **func_key 空间重叠 / 装载边界冲突**（decrypt 库先加载即「活跃」，encrypt 核 launch 被派发到错误 binary 位置 → `c'` 形状对值全错；fresh session 里 decrypt 核不 launch 故恢复）。本仓所有过关 SIM 探针皆单库单 session；decaps 是唯一双库，即差异变量。

**修法（本轮）**：
| 改动 | 文件 |
|------|------|
| decrypt+encrypt+kem **合并单设备库** `ascendc_kernels_${RUN_MODE}` | `cmake/decaps/CMakeLists.txt` |
| 双树同名头**仅 `aiv_func.hpp` 内容分歧**（其余 20 个逐字节相同）→ decrypt 侧改名 `dec_aiv_func.hpp` + 4 包含者改 include；`vendor_sync_from_alg15_decrypt.sh` 每次 sync 后重放该改名 | `vendor/pke_decrypt/compute/{ntt_u,intt_w}/*` · `scripts/vendor_sync_from_alg15_decrypt.sh` |
| 合库后 AIV-only=5（kem_dec_g+prep_a_hat+prep_re+g4_noise+at_r5）触 R1 → `kem_dec_g` 改 **MIX_AIC_1_2 占位**，SIM AIV-only 回落 4 | `kem/f203_kem_dec_g_entry.cpp` |
| 曾尝试 main 单 session `D→G→E→FO`；07-03 后 SIM 默认恢复 2-session，单 session/PhaseE-only 保留为排障开关 | `main_kem_dec_g5_run.cpp` · `run.sh` |

**证据（CPU）**：`KEM_DECAPS_FORCE_REBUILD=1 KEM_DECAPS_VERIFY=1 bash run.sh -r cpu` → `[verify_kem_decaps] K max=0 PASS`；`out/lib/` 仅 `libascendc_kernels_cpu.so`（单库确认）；root 无 stray dump。

**07-03 SIM 根因定位（重要，修正上文乐观预期）**：单库合并解决了「编译期双树头冲突」与「链接期双库」，且 **Phase-D（decrypt→G）在 SIM 单 session 下走通**（`dbg_m_prime/coins/K_prime` 均 max=0）。但完整单 session 的 Phase-E 最终 `c' max=244`、`K max=216`。逐级 dump 证明 `a_hat/re/t_hat/r_hat` 全部 max=0，**首个出错级是 `at_r5`**（`u_hat/tr_hat` max_diff≈3161/3116）。

**PhaseE-only 对照（2026-07-03）**：`KEM_DECAPS_PHASEE_ONLY=1 KEM_DECAPS_SIM_2SESSION=0 KEM_DECAPS_DEBUG=1 KEM_DECAPS_VERIFY=1 bash run.sh -r sim -v Ascend910B4` 跳过 Phase-D，复用上一轮已验证 `m'/K'/coins` 直接跑 Phase-E，结果 **`K max=0 PASS`**，`diag_phase_e.py` 显示 `a_hat/re/t_hat/r_hat/u_hat/tr_hat` 全部 max=0。→ `at_r5` 本身与 Phase-E 链在同一 decaps binary/session 中可正确运行；污染由 **Phase-D 已执行过**这一事实触发，属于 CAModel session 级状态残留，而非 GM 输入、host 同步、LUT 或 `at_r5` 算法错误。

**后续待验（SIM）**：
1. ~~拒绝路径 SIM~~ → **PASS**（`KEM_DECAPS_TAMPER_C=1` 篡改 device `coins[0]`，设备 FO 输出 `J(z‖c)`；CPU 与 SIM 2-session 均已验）。
2. `scripts/liboqs_kem_vs_ascendc.sh` 扩 decaps 段。
3. 若要继续追 CAModel 根因：做 Phase-D 子集二分（只跑 `g4_prep` / `chain_ntt` / `chain_intt` / `kem_dec_g` 后接 Phase-E-only），定位是哪一个 Phase-D kernel 污染后续 `at_r5`。

## 验证

| Gate | 状态 | 证据 |
|------|------|------|
| G4 全链 CPU（单库合并后） | **PASS** | `KEM_DECAPS_FORCE_REBUILD=1 KEM_DECAPS_VERIFY=1 bash run.sh -r cpu` → `K max=0` |
| G4 SIM 合法 c（默认 2-session） | **PASS** | `KEM_DECAPS_VERIFY=1 bash run.sh -r sim` → `K max=0` |
| G4 SIM 单 session（排障） | **c' 污染** | `KEM_DECAPS_SIM_2SESSION=0` → `c' max=244`，`K max=216`；非死锁，~12min 可跑完 |
| PhaseE-only 单 session 对照 | **PASS** | `KEM_DECAPS_PHASEE_ONLY=1 KEM_DECAPS_SIM_2SESSION=0 KEM_DECAPS_DEBUG=1 KEM_DECAPS_VERIFY=1 bash run.sh -r sim` → `K max=0`，`u_hat/tr_hat max=0` |
| 拒绝路径 CPU（设备 FO） | **PASS** | `KEM_DECAPS_TAMPER_C=1 KEM_DECAPS_VERIFY=1 bash run.sh -r cpu` → `REJECT PASS` |
| 拒绝路径 SIM 2-session（设备 FO） | **待验** | 与合法路径同架构；CPU 已 PASS |

## 实现要点（相对 INTEGRATION_PLAN 首版）

| 项 | 规划 | 当前实现 |
|----|------|----------|
| K1 `G` | 嵌入 `chain_intt` 尾 | **独立 AIV launch** `f203_kem_dec_g`（`sync` 后读 `mGm`） |
| K2 FO | 嵌入 `pack` 尾 | **CPU/SIM 均为设备** `f203_kem_dec_pack`（`KemDecFo`）；SIM 2-session 不再 host `memcmp` |
| kernel 库 | 单 `.so` | **已合并单 `.so`**（2026-07-02）；`aiv_func.hpp` 改名解双树头冲突 |
| SIM session | 单 session D+E | **默认 2-session**（`KEM_DECAPS_SIM_2SESSION=1`）；单 session / PhaseE-only 仅作排障 |

## SIM 问题详情（2026-07-02）

### 现象

- 单 session 内 Phase-D 正确、Phase-E 重加密 **`c'≠c`** → FO 走 **`J(z‖c)`** → `K max=216` FAIL。
- **非**随机种子问题：`SEED_D=20260619` 固定；dump 显示 `m'`/`K'`/`coins` 与期望 **max=0**。

### 诊断证据

| 检查 | 结果 |
|------|------|
| SIM dump `m'` / `K'` / `coins` | 与 host 期望 **max=0** |
| 同组 `m'/coins` 单独跑 alg14 G5 SIM | **`c'` max=0 PASS** |
| 单 session 内 `dbg_c_prime` vs `c` | **max=244**（几乎全字节错） |
| 释放 decrypt GM + 重载 encrypt LUT（仍单 session） | **仍 FAIL** |

### 根因结论（2026-07-02 已定位）

**曾用 decrypt/encrypt 双设备库**在同一 ACL session 内 **func_key 空间重叠 / 装载边界冲突**：decrypt 库先加载即「活跃」，encrypt 核 launch 被派发到错误 binary 位置 → `c'` 形状对值全错；fresh session 里 decrypt 核不 launch，encrypt 库独占故恢复。**非** GM 数据、`SEED_D` 或算法参数问题。**合并单设备库**（单 func_key 空间）已消除此「双库」层面冲突（编译期头冲突 + 链接期双 `.so`）。

> ⚠️ 但这**不是**唯一病根，见下节：07-03 SIM 实测显示单库单 session 下 Phase-E 输出仍被污染；“自旋卡死”已修正为慢跑误判。

### 07-03 SIM 实测结论（修正「Phase-E 自旋卡死」误判）

单库合并后 `SIM_DIRECT=1 KEM_DECAPS_VERIFY=1 bash run.sh -r sim`（**单 session**，`KEM_DECAPS_SIM_2SESSION=0`）实测：

| 项 | 结论 |
|----|------|
| **是否真死锁** | **否**。Phase-E 全部 launch 完成（~12min）；`prep_a_hat` 在 Phase-D 后极慢（Alg.7 rej 环活跃自旋），~7min 无输出易被误判为 hang |
| Phase-D | `m'/K'/coins` **max=0** ✓ |
| Phase-E 单 session | **`c' vs c max=244`** → FO 误拒 **`K max=216`** ✗ |
| Phase-D→E hygiene | 释 decrypt GM + 重载 LUT + 重 H2D m'/coins（**仍单 session**）→ **`c' max=244` 不变** |
| **完整 Decaps 可靠 SIM 路径** | `aclFinalize` 后 fresh `run_g5_sim_full`（**2-session**）→ **`K max=0` PASS** |
| **run.sh 默认（2026-07-03）** | SIM 模式 **`KEM_DECAPS_SIM_2SESSION=1`**；CPU 仍为单 session 设备 FO |

**判读**：合库消除双库 func_key 冲突（R3 子类），但 CAModel **超长单 session D→E** 仍污染 Phase-E 输出；非 kernel 同步死锁，而是 **session 级状态 + c' 全错**。真修单 session 需 CAModel 侧根因或 NPU 实机对照。

### 历史 workaround（现降级为非默认回退）

`KEM_DECAPS_SIM_2SESSION=1` 为 SIM 默认可靠路径：Phase-D 后 `aclFinalize`，fresh session 跑 Phase-E + **设备** `f203_kem_dec_pack`（`KemDecFo`，**无 host memcmp**）；`KEM_DECAPS_SIM_2SESSION=0` 与 `KEM_DECAPS_PHASEE_ONLY=1` 仅供排障。

### 与 CPU 路径差异

| 路径 | Session | FO |
|------|---------|-----|
| **CPU** | 单 session 全链 | 设备 `f203_kem_dec_pack`（`CtEqual` + `J(z‖c)` + 选 K） |
| **SIM** | 两段 session | 同上（Phase-E fresh session 内设备 FO） |

**Host 仍参与（非密码学）**：`build_at_r5_mat` 拼 `matM`（与 alg14/20 相同 staging glue）、LUT H2D、最终 `K` D2H 写盘；拒绝路径测试开关 `KEM_DECAPS_TAMPER_C=1` 在 device 上改 `coins[0]`。

## 代码变更摘要

| 路径 | 说明 |
|------|------|
| `kem/f203_kem_dec_g_entry.cpp` | K1 独立 AIV 核 |
| `kem/f203_kem_dec_chain_intt_entry.cpp` | 仅 extract `m'`（不含 G） |
| `kem/f203_kem_dec_pack_entry.cpp` | pack + FO（CPU 路径） |
| `main_kem_dec_g5_run.cpp` | CPU 单 session；SIM 默认 2-session；`KEM_DECAPS_SIM_2SESSION=0` / `KEM_DECAPS_PHASEE_ONLY=1` 排障 |
| `cmake/decaps/CMakeLists.txt` | **decrypt+encrypt 合并单库** `ascendc_kernels_${RUN_MODE}` |
| `vendor/pke_decrypt/compute/ntt_u/dec_aiv_func.hpp` | 由 `aiv_func.hpp` 改名（解双树同名头冲突）；4 包含者改 include |
| `scripts/vendor_sync_from_alg15_decrypt.sh` | sync 后重放 `dec_aiv_func` 改名（幂等） |
| `run.sh` | 单库名 SKIP_REBUILD；`KERNEL_COMPUTE_BUDGET_SEC=1800` |

## 遗留

1. **真修**单 session SIM Phase-E（对齐 INTEGRATION_PLAN §4.1）。
2. SIM 2-session **拒绝路径**长测（架构同合法路径，CPU 已 PASS）。
3. 可选：device 侧 `prep_matM` 核，消除 `build_at_r5_mat` host staging。
4. `nm` func_key 分库审计。
5. ~~`scripts/liboqs_kem_vs_ascendc.sh` decaps 段~~ → **已扩四阶段**（KeyGen→Encaps→Decaps→reject），CPU 全绿；另见纯 device 闭环 `scripts/roundtrip_kem_keygen_encaps_decaps.sh`。SIM 待长测。

## 端到端测试脚本（仓库级 `scripts/`）

| 脚本 | 作用 |
|------|------|
| `liboqs_kem_vs_ascendc.sh` + `..._verify.py` + `liboqs_kem_fixture.py` + `liboqs_kem_ref.c` | 四阶段逐级对 liboqs：ek/dk/c/K/K(decaps)/K(reject) 全 max=0 + shared-secret agreement |
| `roundtrip_kem_keygen_encaps_decaps.sh` + `roundtrip_kem_verify.py` | 纯 device 闭环 `Decaps(Encaps.c)==Encaps.K`；拒绝 `K==J(z‖c)` 且≠K_enc（不借 liboqs） |
| `liboqs_kem_decaps_batch.sh` + `kat_liboqs_kem_decaps.py` | **分项 kat**：固定 stash `dk`；每轮 liboqs `encaps_derand(ek,m)` 造 `c`，device Decaps 对拍 `K`（不跑 device Encaps）；**CPU×10 + SIM×1 PASS**（2026-07-03） |

**2026-07-03 分项 kat 修复**：SIM clean build 曾因 `vendor/pke_encrypt/prep/a_hat/alg7/f203_alg7_rej_scalar.c.o`
进入 `ascendc_library` 的 AIC/AIV object 合并链而报 `unknown file type`。该 `.c` 是 CPU/参考语义文件，
设备热路径使用 `.hpp` 内联逻辑；现仅 CPU twin 编入，SIM/NPU 设备库剔除该 `.c`。复验：
`KEM_DECAPS_TRACE=1 KEM_DEC_VERBOSE=1 KEM_DEC_CPU_TRIALS=0 KEM_DEC_SIM_TRIALS=1 bash scripts/liboqs_kem_decaps_batch.sh`
→ `SIM 1/1 OK`，`Decaps K match liboqs`。

## 验收命令

```bash
# 前置（各跑一次即可；后续 SKIP_REBUILD）
cd ../fix-f203-alg19-kem-keygen-k4 && bash run.sh -r cpu -v Ascend910B4
cd ../fix-f203-alg20-kem-encaps-k4 && KEM_ENCAPS_SKIP_REBUILD=1 bash run.sh -r cpu -v Ascend910B4

# Decaps
cd ../fix-f203-alg21-kem-decaps-k4
KEM_DECAPS_VERIFY=1 bash run.sh -r cpu -v Ascend910B4
KEM_DECAPS_SKIP_REBUILD=1 KEM_DECAPS_VERIFY=1 bash run.sh -r sim -v Ascend910B4
```

方案见 [`INTEGRATION_PLAN.md`](INTEGRATION_PLAN.md) · 约束见 [`SELF_CONTAINED.md`](SELF_CONTAINED.md)。
