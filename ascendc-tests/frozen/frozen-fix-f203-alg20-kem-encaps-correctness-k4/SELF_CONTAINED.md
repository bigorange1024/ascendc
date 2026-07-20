# 自包含与设备全链约束 — fix-f203-alg20-kem-encaps-correctness-k4

对齐 alg19 KeyGen / alg14 Encrypt 治理：**liboqs 不得渗入默认 `run.sh`**；**Encaps 密码学全在 device**。

## 1. 自包含（除 `library/shared`）

| 允许 | 禁止 |
|------|------|
| 本目录 `vendor/`、`cmake/`、`scripts/` | `#include` / import **其它探针或 example** 源码路径 |
| `#include` **`library/shared/`** | 运行时 `LD_LIBRARY_PATH` → `thirdparty/liboqs` |
| 仓库级 `scripts/sim_env.sh`、`kernel-run-timeout.sh` | **`oqs.h` 出现在默认验收路径** |
| 从 **frozen alg14 G5** `vendor_sync` 到 `vendor/pke_encrypt/`（非 stable Encrypt） | 子进程调 frozen/stable `run.sh` 作默认全链 |
| **`gen_data` 只读复制** alg19 的 `output/ek_kem.bin` 到 `input/` | 本探针内嵌 Alg.19 KeyGen launch |

**公钥**：`input/ek_kem.bin` 是 **KEM KeyGen 产物文件**，不是 Host 重算或 liboqs 生成。

## 2. 设备全链（无 Host 密码学）

**生产路径**（默认 `bash run.sh`）：

```text
input/  ek_kem.bin（自 alg19 复制）+ seed_d.bin + LUT
   → Launch…（设备：Alg.20 = UB 内 m + H/G + Alg.14 Encrypt）
output/ c.bin (1568B) + K.bin (32B)
```

| 允许（Host） | 禁止（生产路径） |
|--------------|------------------|
| `gen_data.py` 复制 **alg19 `ek_kem.bin`**、写 `seed_d`、生成 LUT | Host 调 `OQS_KEM_encaps` / liboqs 写 `c`/`K` |
| `KEM_ENCAPS_VERIFY=1`：对拍 golden / 仓库 `liboqs_kem_*` | Host `tiny_sha3` 算 `G(m‖H(ek))` 冒充设备 |
| `scripts/host_golden/` 分阶段期望 | **D2H / 落盘 `m[32]`** |
| | Host 预填 `coins.bin` 进默认全链（`r` 须来自 device `G`） |

## 3. Golden 分层

| 层级 | 路径 | 用于 |
|------|------|------|
| 分阶段 | `scripts/host_golden/gate_*.py` | G1 Encrypt · G2 H/m/G · G3 c+K |
| 端到端 | `scripts/host_golden/golden_c_K.py` | `KEM_ENCAPS_VERIFY=1` |
| 外部 L2 | 仓库 `scripts/liboqs_kem_vs_ascendc.sh` encaps 段（**待扩**） | liboqs `encaps_derand`；**不进**本目录 `run.sh` |

## 4. 审查命令

```bash
rg -i 'liboqs|oqs\.h|PQCP_MLKEM' .
rg '#include.*ascendc-tests/(pass|fix)-' . --glob '*.{cpp,hpp}'
rg '#include.*examples/' . --glob '*.{cpp,hpp}'
```
