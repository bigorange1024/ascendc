# SELF_CONTAINED — fix-f203-alg14-encrypt-2launch-k4

本探针**自包含**：默认 `run.sh` 全链密码学在 AI Core，Host 仅写种子类 input、读 output。

## 允许的外部依赖

| 类型 | 说明 |
|------|------|
| `library/shared` | 编译期共享头（如有） |
| 仓库 `scripts/` | CANN 环境壳、`kernel-run-timeout.sh`、`camodel_sim_log.sh`、`xelatex-clean.sh` |
| 注释中的文档链接 | `docs/`、`qa/`、其它探针路径仅作**文档引用**，不得 `#include` |

## 禁止

| 禁止 | 检查 |
|------|------|
| `thirdparty/liboqs`、`oqs.h`、`PQCP_MLKEM_*`、`indcpa_enc` 进入 `run.sh`/`main`/设备核 | `rg -i 'liboqs\|oqs\.h\|PQCP_MLKEM\|indcpa_enc'` |
| `#include` 其它探针/example 源码路径 | `rg '#include.*ascendc-tests/(pass-\|fix-)' prep compute pack *.cpp`（本目录除外） |
| golden 作为生产输入源 | `scripts/host_golden/` 仅 `ENCRYPT_VERIFY=1` / 分阶段 gate |

## Vendored 规则

从活跃探针**复制**源文件到本目录子树（`prep/ compute/ pack/`），改 include 为相对路径；
计算核来源限**活跃**探针（旧 encrypt 探针、keygen、stage123、innerproduct、compress-d、byteencode-d、alg7、cbd-eta2），
**禁止**从 `frozen/` 复制。
