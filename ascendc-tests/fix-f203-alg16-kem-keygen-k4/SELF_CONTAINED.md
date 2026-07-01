# 自包含与设备全链约束 — fix-f203-alg16-kem-keygen-k4

对齐 PKE Encrypt/Decrypt 探针治理：**外部黑盒（liboqs）不得渗入默认 `run.sh`**；**KEM 密码学全在 device**。

## 1. 自包含（除 `library/shared`）

| 允许 | 禁止 |
|------|------|
| 本目录 `prep/`、`cmake/`、`scripts/`、`vendor/` | `#include` / Python import **其它探针或 example** 源码路径 |
| 编译期 `#include` **`library/shared/`** | 运行时 `LD_LIBRARY_PATH` 指向 `thirdparty/liboqs` |
| 仓库级 `scripts/sim_env.sh`、`kernel-run-timeout.sh` | **`oqs.h`、`PQCP_MLKEM_*` 出现在默认验收路径** |
| 从 **stable / 活跃探针 vendor 复制** launch 编排与 kernel 到本目录 `vendor/` | 默认全链 `bash ../stable-*/run.sh` 或子进程调其它探针 |

**规则**：需要的能力 **vendor 到本目录** 或经 **`library/shared`**；不得跨目录 `#include` 活跃探针路径。

## 2. 设备全链（无 Host 密码学）

**生产路径**（默认 `bash run.sh`）：

```text
input/  seed_d.bin（或契约约定的 derand 种子）
   → Launch…（设备：Alg.13 PKE KeyGen + Alg.16 增量）
output/ ek_kem.bin (1568B) + dk_kem.bin (3168B，liboqs 展开布局)
```

| 允许（Host） | 禁止（生产路径） |
|--------------|------------------|
| `gen_data.py` 写 **合法 input**（固定 `SEED_D` 等） | Host 调 `OQS_KEM_keypair` / liboqs 写 `ek`/`dk` |
| `KEM_KEYGEN_VERIFY=1`：对拍 `golden_*.bin` 或仓库 `scripts/liboqs_kem_*`（VERIFY 专用） | Host `tiny_sha3` 算 `H(ek)` / 拼 `dk_kem` 冒充设备 |
| `scripts/host_golden/`：分阶段期望（禁 liboqs API） | 子进程 `cd` 其它探针 `run.sh` 作默认全链 |

## 3. Golden 分层

| 层级 | 路径 | 用于 |
|------|------|------|
| 分阶段 | `scripts/host_golden/gate_*.py` | G1 PKE 段 · G2 H/z · G3 拼接 |
| 端到端 | `scripts/host_golden/golden_ek_dk.py` | `KEM_KEYGEN_VERIFY=1` |
| 外部 L2 | 仓库 `scripts/liboqs_kem_vs_ascendc.sh`（**待建**） | liboqs KEM keypair 对拍；**不进**本目录 `run.sh` |

## 4. 审查命令

```bash
rg -i 'liboqs|oqs\.h|PQCP_MLKEM' .
rg '#include.*ascendc-tests/(pass|fix)-' . --glob '*.{cpp,hpp}'
rg '#include.*examples/' . --glob '*.{cpp,hpp}'
```
