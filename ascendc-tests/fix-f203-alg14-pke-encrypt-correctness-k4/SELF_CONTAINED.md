# 自包含与设备全链约束 — fix-f203-alg14-pke-encrypt-correctness-k4

对齐 KeyGen pass 探针教训：**外部黑盒（liboqs）渗入生产路径后难以清干净**。

## 1. 自包含（除 `library/shared`）

| 允许 | 禁止 |
|------|------|
| 本目录 `prep/`、`compute/`、`pack/`、`cmake/`、`scripts/`、`thirdparty/` | `#include` / Python import **其它探针或 example** 源码路径 |
| 编译期 `#include` **`library/shared/`** | 运行时 `LD_LIBRARY_PATH` 指向 `thirdparty/liboqs` |
| 仓库级 `scripts/sim_env.sh`、`kernel-run-timeout.sh` | **`oqs.h`、`PQCP_MLKEM_*`、`kat_liboqs_*` 出现在默认验收路径** |

**规则**：从活跃探针 **复制** 到本目录再改；需要的能力不得跨目录 `#include`。

## 2. 设备全链（无 Host 密码学）

**生产路径**（默认 `bash run.sh`）：

```text
input/  ek_pke.bin + m.bin + coins.bin
   → Launch…（设备：Alg.14 各段，见 INTEGRATION_PLAN §4）
output/ c.bin
```

| 允许（Host） | 禁止（生产路径） |
|--------------|------------------|
| `gen_data.py` 写 **合法 input**（ek 可用 `host_golden` **离线**生成，不进入 kernel） | Host 调 `indcpa_enc` / liboqs 写 `c.bin` |
| `ENCRYPT_VERIFY=1`：对拍 `golden_c.bin` | 默认 `run.sh` 依赖外部 KEM 完成加密 |
| `scripts/host_golden/`：分阶段期望（抄 C ref 可，**禁 liboqs API**） | 把 liboqs 当「临时 Phase0」长期留在 `main` |

## 3. Golden 分层

| 层级 | 路径 | 用于 |
|------|------|------|
| 分阶段 | `scripts/host_golden/gate_*.py` | G1–G4 中间张量 |
| 端到端 | `scripts/host_golden/golden_c.py` | `ENCRYPT_VERIFY=1` 全链 |
| 外部 | **无**（不建 `kat_liboqs_vs_ascendc.sh`） | — |

## 4. 审查命令

```bash
rg -i 'liboqs|oqs\.h|PQCP_MLKEM' .
rg '#include.*ascendc-tests/(pass|fix)-' prep compute pack *.cpp 2>/dev/null
ls input/   # G5：仅 ek_pke.bin m.bin coins.bin
```
