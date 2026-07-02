# 自包含与设备全链约束 — fix-f203-alg21-kem-decaps-k4

对齐 alg19/20/alg14/15 探针治理：**外部黑盒不得渗入默认 `run.sh`**；**Decaps 密码学全在 device**（见 §3 SIM 例外说明）。

## 1. 自包含（除 `library/shared`）

| 允许 | 禁止 |
|------|------|
| 本目录 `vendor/`、`kem/`、`cmake/`、`scripts/` | `#include` / import **其它探针或 example** 源码路径 |
| `library/shared/`（SHA3/SHAKE） | 运行时 `liboqs` 进默认 `run.sh` |
| `vendor_sync_from_alg14_encrypt.sh` + `vendor_sync_from_alg15_decrypt.sh` | 子进程调 alg14/15/19/20 的 `run.sh` 作生产全链 |
| 仓库 `scripts/sim_env.sh`、`kernel-run-timeout.sh` | Host `tiny_sha3` / liboqs 写 `K.bin` |
| vendored `main_encrypt_g5_run.cpp`（**仅 SIM host 编排**，非跨探针 `#include`） | — |

## 2. 设备全链（无 Host 密码学）

**生产 I/O**：

```text
input/  dk_kem.bin (3168B) + c.bin (1568B) + LUT
   → Decaps 密码学 → output/K.bin (32B)
```

| 允许（Host） | 禁止（生产路径） |
|--------------|------------------|
| `gen_data.py` 从 alg19/20 **复制** `dk_kem`/`c`（离线） | Host 跑 indcpa_dec + indcpa_enc 拼 FO |
| `KEM_DECAPS_VERIFY=1` 对拍 `golden_K`（来自 alg20 `K.bin`） | 默认路径落盘 `m'`/`c'`（调试用 `output/dbg_*` 非默认） |

## 3. CPU vs SIM 路径（2026-07-02 首版）

| 模式 | Session | Phase-D/K1 | Phase-E | FO |
|------|---------|------------|---------|-----|
| **CPU** | **单 session** | device Decrypt + `f203_kem_dec_g` | device Encrypt G5 | device `f203_kem_dec_pack` |
| **SIM** | **两段 session** | session-1：device D+G | session-2：vendored `run_g5_sim_full` | **host `memcmp(c,c')` + `K'`** |

**SIM 例外原因**：CAModel 单 session 内 Decrypt 后立即 Encrypt 导致 `c'` 污染（`m'/coins` 已证正确；同输入单独 alg14 G5 SIM PASS）。详见 [`STATUS.md`](STATUS.md) §SIM 问题详情。

**审查含义**：

- CPU 路径满足「单 session 设备全链 + 设备 FO」。
- SIM 路径当前为 **有条件完成**：合法 `c` 路径 PASS；**拒绝路径与设备 FO 在 SIM 未验收**。
- 后续须 **真修单 session SIM** 或 **补设备 FO 单测**，再收回本例外。

## 4. Golden 分层

| 层级 | 用途 |
|------|------|
| `scripts/verify_kem_decaps.py` | `KEM_DECAPS_VERIFY=1`：对拍 alg20 `golden_K.bin` |
| `output/dbg_*.bin` | 开发诊断（非默认 `run.sh` 产物） |
| `scripts/liboqs_kem_vs_ascendc.sh` decaps 段 | L2（未实现） |

## 5. 审查

```bash
rg -i 'liboqs|oqs\.h' . --glob '!scripts/host_golden/**'
rg '#include.*ascendc-tests/(pass|fix)-' . --glob '*.{cpp,hpp}'
```
