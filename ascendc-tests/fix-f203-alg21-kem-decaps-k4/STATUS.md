# STATUS — fix-f203-alg21-kem-decaps-k4

FIPS 203 **Algorithm 21 `ML-KEM.Decaps(dk, c)`**（**ml_kem_1024 / k=4**）。

| 项 | 值 |
|---|---|
| **阶段** | **单设备库合并版**（2026-07-02 家里）：CPU 单 session `K max=0` **PASS**；SIM 单 session + `nm` 审计**待公司验证** |
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
| main **默认单 session** `D→G→E→FO`；两段 session 降级为 `KEM_DECAPS_SIM_2SESSION=1` 非默认回退 | `main_kem_dec_g5_run.cpp` · `run.sh` |

**证据（CPU）**：`KEM_DECAPS_FORCE_REBUILD=1 KEM_DECAPS_VERIFY=1 bash run.sh -r cpu` → `[verify_kem_decaps] K max=0 PASS`；`out/lib/` 仅 `libascendc_kernels_cpu.so`（单库确认）；root 无 stray dump。

**家里 07-03 SIM 实测结论（重要，修正上文乐观预期）**：单库合并解决了「编译期双树头冲突」与「链接期双库」，且 **Phase-D（decrypt→G）在 SIM 单 session 下走通**（`dbg_m_prime/coins/K_prime` 均产出）。但进入 **Phase-E（Re-Encrypt）后一个 vector core 在 CAModel 内无限自旋**（`sim_log/core0.veccore0.instr_log.dump` 持续暴涨、单核 65MB+，~7min 不退），已手动终止。→ 「双库 func_key 冲突」不是唯一病根；**Phase-E 单 session 重加密链本身在 SIM 存在死锁/坏循环边界**（正是当初两段 session workaround 想绕开处）。公司排查应聚焦 **Phase-E 首个 launch 的 kernel 同步点 / 循环终止条件**，而非仅 nm func_key。

**公司待验（SIM）**：
1. `SIM_DIRECT=1 KEM_DECAPS_VERIFY=1 bash run.sh -r sim -v Ascend910B4` → 默认单 session。**当前家里实测卡死在 Phase-E**；需定位 Phase-E 首个自旋 kernel（看 `sim_log/core0.veccore0.instr_log.dump` 停在哪条指令）。若暂不解，可用 `KEM_DECAPS_SIM_2SESSION=1` 两段 session 回退先验 `K max=0`。
2. `nm build/CMakeFiles/ascendc_kernels_sim_aiv_device_dir/device_aiv.o | grep funckey` → **AIV-only ≤ 4**；若 507000 说明仍 >4，再挑一个数据通路 AIV 核改 MIX。
3. 拒绝路径 SIM（篡改 `c` 一字节 → `K = J(z‖c)`）。
4. `scripts/liboqs_kem_vs_ascendc.sh` 扩 decaps 段。
5. 单库 SIM 稳定后可删 `KEM_DECAPS_SIM_2SESSION` 回退与 `main_encrypt_g5_run.cpp` 链接。

## 验证

| Gate | 状态 | 证据 |
|------|------|------|
| G4 全链 CPU（单库合并后） | **PASS** | `KEM_DECAPS_FORCE_REBUILD=1 KEM_DECAPS_VERIFY=1 bash run.sh -r cpu` → `K max=0` |
| G4 SIM 单 session（合法 c） | **卡死（家里 07-03 实测）** | 合库编译/链接成功、Phase-D 走通（`output/dbg_{m_prime,coins,K_prime}.bin` 产出）；进入 **Phase-E 重加密后 veccore0 `instr_log` 无限自旋**（单核 65MB+ 且持续增长，~7min 无退出，255% CPU），非 `max=244` 输出污染而是**死循环**；已手动终止。详见「SIM 问题详情 07-03」 |
| func_key 审计（`nm`） | **待公司** | 合库 AIV-only 预期=4（kem_dec_g 已改 MIX）；≥5 则再挑数据通路核改 MIX |
| 拒绝路径 SIM | **未验** | 篡改 `c` 一字节 → `K=J(z‖c)` |

## 实现要点（相对 INTEGRATION_PLAN 首版）

| 项 | 规划 | 当前实现 |
|----|------|----------|
| K1 `G` | 嵌入 `chain_intt` 尾 | **独立 AIV launch** `f203_kem_dec_g`（`sync` 后读 `mGm`） |
| K2 FO | 嵌入 `pack` 尾 | CPU：**设备** `f203_kem_dec_pack`；SIM：**host `memcmp(c,c')` + 取 `K'`**（workaround） |
| kernel 库 | 单 `.so` | **已合并单 `.so`**（2026-07-02）；`aiv_func.hpp` 改名解双树头冲突 |
| SIM session | 单 session D+E | **默认单 session**（合库后）；两段 session 降为 `KEM_DECAPS_SIM_2SESSION=1` 回退 |

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

> ⚠️ 但这**不是**唯一病根，见下节：07-03 SIM 实测显示单库单 session 下 Phase-E 仍自旋卡死。

### 07-03 SIM 实测：Phase-E 自旋卡死（家里，未解）

单库合并后 `SIM_DIRECT=1 KEM_DECAPS_FORCE_REBUILD=1 KEM_DECAPS_VERIFY=1 bash run.sh -r sim` 实测：

| 阶段 | 现象 |
|------|------|
| 编译 / 链接 | **OK**，产出单 `libascendc_kernels_sim.so` + `ascendc_kem_decaps_bbit` |
| Phase-D（decrypt→G） | **走通**：`output/dbg_{m_prime,coins,K_prime}.bin` 于 kernel 阶段产出 |
| Phase-E（Re-Encrypt） | **卡死**：`sim_log/core0.veccore0.instr_log.dump` 持续暴涨（单核 65MB+），进程 255% CPU 自旋 ~7min 无退出 → 手动 `kill` |

**判读**：不是 `max=244` 输出污染（那是双库症状，已消除），而是 **Phase-E 重加密链在 CAModel 单 session 下死循环/等不到同步**——这正是历史两段 session workaround 当初想绕开的点。**下一步（公司）**：从 `core0.veccore0.instr_log.dump` 末尾定位自旋指令 / Phase-E 首个 launch 的 kernel 同步点与循环终止条件；或先用 `KEM_DECAPS_SIM_2SESSION=1` 两段 session 回退验 `K max=0` 保底。

### 历史 workaround（现降级为非默认回退）

`KEM_DECAPS_SIM_2SESSION=1` 保留原两段 session（`aclFinalize` 后 fresh `run_g5_sim_full` + host `memcmp`），仅供对照/排障；默认已走单库单 session 设备 FO。

### 与 CPU 路径差异

| 路径 | Session | FO |
|------|---------|-----|
| **CPU** | 单 session 全链 | 设备 `f203_kem_dec_pack`（比对 + `J` + 选 K） |
| **SIM** | 两段 session | Host 比对 + 取 `K'`（拒绝路径未验） |

## 代码变更摘要

| 路径 | 说明 |
|------|------|
| `kem/f203_kem_dec_g_entry.cpp` | K1 独立 AIV 核 |
| `kem/f203_kem_dec_chain_intt_entry.cpp` | 仅 extract `m'`（不含 G） |
| `kem/f203_kem_dec_pack_entry.cpp` | pack + FO（CPU 路径） |
| `main_kem_dec_g5_run.cpp` | CPU 单 session；SIM **默认单 session** + `KEM_DECAPS_SIM_2SESSION=1` 回退 |
| `cmake/decaps/CMakeLists.txt` | **decrypt+encrypt 合并单库** `ascendc_kernels_${RUN_MODE}` |
| `vendor/pke_decrypt/compute/ntt_u/dec_aiv_func.hpp` | 由 `aiv_func.hpp` 改名（解双树同名头冲突）；4 包含者改 include |
| `scripts/vendor_sync_from_alg15_decrypt.sh` | sync 后重放 `dec_aiv_func` 改名（幂等） |
| `run.sh` | 单库名 SKIP_REBUILD；`KERNEL_COMPUTE_BUDGET_SEC=1800` |

## 遗留

1. **真修**单 session SIM Phase-E（对齐 INTEGRATION_PLAN §4.1）。
2. SIM **拒绝路径**（`KEM_DECAPS_VERIFY=2` 或设备 FO）。
3. `nm` func_key 分库审计。
4. `scripts/liboqs_kem_vs_ascendc.sh` decaps 段。

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
