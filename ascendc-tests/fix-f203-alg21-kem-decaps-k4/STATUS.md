# STATUS — fix-f203-alg21-kem-decaps-k4

FIPS 203 **Algorithm 21 `ML-KEM.Decaps(dk, c)`**（**ml_kem_1024 / k=4**）。

| 项 | 值 |
|---|---|
| **阶段** | 首版实现；**G4 CPU+SIM PASS**（合法 `c` 路径 `K` 与 Encaps 一致） |
| **I/O** | `dk_kem` **3168B** · `c` **1568B** · `K` **32B** |
| **输入来源** | alg19 `dk_kem.bin` + alg20 `c.bin`（`SEED_D=20260619`） |
| **SIM tick** | Phase-D **~534k** + fresh Phase-E **~899k**（两段 session 合计；非原规划单 session ~1.43M） |

## 验证

| Gate | 状态 | 证据 |
|------|------|------|
| G4 全链 CPU | **PASS** | `KEM_DECAPS_VERIFY=1 bash run.sh -r cpu` → `K max=0` |
| G4 SIM（合法 c） | **PASS** | `KEM_DECAPS_VERIFY=1 bash run.sh -r sim` → `K max=0` |
| G1–G3 分段 gate 脚本 | 未跑 | 首版直 G4 |
| func_key 审计 | 待跑 | decrypt/encrypt **分库**（4+1 decrypt AIV + encrypt 侧含 pack 双份） |
| 拒绝路径 SIM | **未验** | 见 §SIM  workaround |

## 实现要点（相对 INTEGRATION_PLAN 首版）

| 项 | 规划 | 当前实现 |
|----|------|----------|
| K1 `G` | 嵌入 `chain_intt` 尾 | **独立 AIV launch** `f203_kem_dec_g`（`sync` 后读 `mGm`） |
| K2 FO | 嵌入 `pack` 尾 | CPU：**设备** `f203_kem_dec_pack`；SIM：**host `memcmp(c,c')` + 取 `K'`**（workaround） |
| kernel 库 | 单 `.so` | **decrypt / encrypt 分库**（避免 `tiling` 头重定义） |
| SIM session | 单 session D+E | **两段 session**：D+G → `aclFinalize` → fresh alg14 `run_g5_sim_full` |

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

### 根因结论（当前置信度）

**CAModel/ACL 超长单 session 内「Decrypt 后立即 Encrypt」的状态问题**——不是输入数据错、不是 `SEED_D` 不一致。确切污染点（哪个 GM/全局 SIM 状态）**尚未定位**。

### 当前 workaround（SIM 合法路径）

1. Phase-D + `f203_kem_dec_g` 于 **session-1** 完成；D2H 校验 `m'/K'/coins`（调试 dump）。
2. `aclFinalize` 后 **session-2** 调用 vendored `run_g5_sim_full(ek, coins, m, …)` 得 `c'`。
3. Host **`memcmp(c, c')==0`** → 输出 `K'`；否则 **return 61**（**未**在 SIM 跑设备 `J`）。

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
| `main_kem_dec_g5_run.cpp` | CPU 单 session；SIM 两段 + fresh encrypt |
| `cmake/decaps/CMakeLists.txt` | decrypt/encrypt 分库；SIM 链 `main_encrypt_g5_run.cpp` |
| `scripts/vendor_sync_from_alg14_encrypt.sh` | 同步 `main_encrypt_g5_run.cpp` + `f203_encrypt_g5_run.hpp` |
| `run.sh` | `KEM_DECAPS_SKIP_REBUILD=1` 默认；`KERNEL_COMPUTE_BUDGET_SEC=1800` |

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
