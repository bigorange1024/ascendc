# Cursor Cloud 环境说明

本文件面向在 **Cursor Cloud VM**（云端 Agent）下工作的协作者/AI Agent，记录本仓在该环境
（**非 WSL**，Ubuntu 24.04 x86_64、无昇腾 NPU）下的启动/运行注意点。仓库总览与阅读顺序见
`README.md`；全仓底线见 `.cursor/rules/ascendc-development.mdc`；本机 WSL 环境复现细节见
`docs/engineering/环境复现与开发指南.md`。三环境分流与 `-r auto|verify` 见
[`docs/engineering/NPU真机环境说明.md`](docs/engineering/NPU真机环境说明.md)（探测脚本
[`scripts/runtime_env.sh`](scripts/runtime_env.sh)）。依赖安装由 Cloud 启动更新脚本完成，此处不重复。

## 环境概况

- 代码在 `/workspace`；但仓库大量 `run.sh` 硬编码 `${HOME}/ascendc/...`（如 `scripts/env.sh`）。
  因此必须有软链 **`~/ascendc -> /workspace`**（由更新脚本维护）。
- CANN 社区版 **9.0.0**（Toolkit + 910-ops）装在 `~/Ascend/cann`，由 VM 快照保存，**不**在更新脚本里重装。
  自检：`bash scripts/verify-cann.sh`（应 `version=9.0.0`、`ccec`、`tikicpulib: OK`、`simulator libs`）。
- `npu` 模式不可用（无实机）。CPU 孪生（`-r cpu`）与 CAModel 仿真（SIM）均可用。
- 探测：`runtime_env_detect` 在此类主机通常为 `HOST_KIND=cloud`、`HAS_NPU=0`；试点 `run.sh -r auto` → `sim`（有 SIM 库时）。

## 非显然的坑（Cloud VM 特有）

- **`/etc/ascend_install.info` 必须存在且含 `Driver_Install_Path_Param=` 行。** 两个原因：
  1. `scripts/sim_env.sh` 要求该文件存在，否则 SIM 的 `aclInit` 失败并直接报错。
  2. `~/Ascend/cann/set_env.sh` 会执行 `driver_install_path_param=$(grep -iw driver_install_path_param /etc/ascend_install.info | cut ...)`。
     `add_custom/run.sh` 在 `set -euo pipefail` 下 source `env.sh`，若该文件缺少此键，`grep` 无匹配 +
     `pipefail` 会让整段 source **静默失败**（run.sh RC=1、无输出）。含该键即可避免。

  本 VM 已写入（真实文件，非软链），内容形如：
  ```
  Install_Path=/home/ubuntu/Ascend
  Toolkit_InstallPath=/home/ubuntu/Ascend/cann-9.0.0
  Driver_Install_Path_Param=/usr/local/Ascend
  ```
  该 driver 路径并不存在，`set_env.sh` 有 `[ -d ]` 保护，无副作用。若快照丢失此文件，用
  `sudo tee /etc/ascend_install.info` 按上面内容重建。

- **系统 C++ 工具链**：默认 `/usr/bin/c++` 是 **clang-18**，它选用 **gcc-14** 的 libstdc++。
  若编译报 `cannot find -lstdc++`，说明缺 `libstdc++-14-dev`（仅装 gcc-13 版不够）。
- **`/usr/bin/time`（GNU time 包）** 被 `examples/stable/*/run.sh` 用于包裹 kernel 运行；缺失会导致
  「/usr/bin/time: No such file or directory」且不产出结果。

## 运行/验收（标准命令见各目录，不在此重复）

- 冒烟：`cd ascendc-tests/add_custom && ./run.sh cpu 910B4`、`SIM_DIRECT=1 ./run.sh sim 910B4`
  （`add_custom` 的 SIM 走较老的 camodel 直连路径，本 VM **可正常跑通**）。
- ML-KEM stable 算子（如 PKE Decrypt）：`bash run.sh -r cpu -v Ascend910B4` 开箱即用（CPU 孪生 =
  辅助正确性参考，golden `[verify] PASS max=0`）。

## SIM（stable/exp 新版 aclrtlaunch 路径）：成因与已实施修复

**现状：标准 `bash run.sh -r sim -v Ascend910B4` 在本 VM 已可通过**（见下方修复）。

成因（两个叠加，都指向同一个 CANN 侧 FPE）：

1. `scripts/sim_env.sh` 会把 `scripts/stub_libascend_dump.cpp` 编出的 **FPE 桩** `libascend_dump.so`
   预置到 `out/lib` 优先加载。该桩缺少**仅**真库导出的 `toolkit::aicpu::dump::Output::InternalSwap`
   等符号 → 非 WSL 上 `libge_common_base.so` 运行期报 `undefined symbol …InternalSwap…`。
2. 若干脆移开桩、让真库加载：`camodel_sim_log.sh` 默认设 `ASCEND_WORK_PATH` → 真库 DumpManager
   `EnableDfxDumper` → `cce::runtime::Config::InitHardwareInfo950()` 对空 map 取模除零 → **SIGFPE**
   （崩在 `main` 之前，与 kernel 数值无关；`camodel_sim_log.sh` 注释已记录）。

**修复（已在 `scripts/sim_env.sh`，非 WSL 自动生效，WSL 行为不变）**：

- 非 WSL（`/proc/version` 无 `microsoft/wsl`）默认**不装桩** → 真实 `libascend_dump.so` 提供全部符号；
- 并默认 `CAMODEL_SKIP_ADX_WORK_PATH=1`（不设 `ASCEND_WORK_PATH`）→ 不触发 DumpManager FPE；
- 覆盖开关：`SIM_DUMP_STUB=1` 强制装桩、`=0` 强制不装（默认 `auto` 按是否 WSL）。

已验证（标准命令，非绕过）：

- `examples/incubating/exp-fips203-mlkem-kem-keygen-k4`：`Total tick 706880`、`KEM KeyGen overall PASS`。
- `examples/stable/stable-fips203-mlkem-pke-decrypt-k4`：`Total tick 283562`、`[verify] PASS max=0`，
  且用例根**无 stray dump**（跳过 ADX work path 的附带好处）。

前置：带 golden/KAT 的用例需先 `bash scripts/clone-thirdparty.sh`（或 `ONLY=liboqs … + build-liboqs.sh`）
备好 `thirdparty/liboqs` 与 `scripts/liboqs_kem_ref`。**`ntt_onnx` 保持私有**；Cloud 须配置 Secrets **`ASCENDC_GH_PAT`**
（详见 [`docs/engineering/thirdparty-本地依赖.md`](docs/engineering/thirdparty-本地依赖.md)）。**勿**把仓改 public。

## Cloud 拉取私有 `ntt_onnx`

1. 创建 fine-grained PAT：仅 `bigorange1024/ntt_onnx`、**Contents: Read**  
2. Cursor Dashboard → Cloud Agents → Secrets → `ASCENDC_GH_PAT=<pat>`（**不要**只靠 `GH_TOKEN`）  
3. Agent 内：

```bash
FORCE=1 ONLY=ntt_onnx BUILD_LIBOQS=0 bash scripts/clone-thirdparty.sh
test -f thirdparty/ntt_onnx/include/mlkem/stable/transpose_mlkem_luts_i8.h && echo OK
```

