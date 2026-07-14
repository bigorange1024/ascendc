# AGENTS.md

本文件面向在本仓库工作的 AI Agent。仓库总览与阅读顺序见 `README.md`；全仓底线见
`.cursor/rules/ascendc-development.mdc`；环境复现细节见
`docs/engineering/环境复现与开发指南.md`。本文件只补充 **Cursor Cloud VM** 环境下的非显然要点。

## Cursor Cloud specific instructions

面向「更新脚本已跑过」的后续 Cloud Agent，记录本仓在 Cloud VM（**非 WSL**，Ubuntu 24.04 x86_64、
无昇腾 NPU）下的启动/运行注意点。依赖安装本身由启动更新脚本完成，此处不重复。

### 环境概况

- 代码在 `/workspace`；但仓库大量 `run.sh` 硬编码 `${HOME}/ascendc/...`（如 `scripts/env.sh`）。
  因此必须有软链 **`~/ascendc -> /workspace`**（由更新脚本维护）。
- CANN 社区版 **9.0.0**（Toolkit + 910-ops）装在 `~/Ascend/cann`，由 VM 快照保存，**不**在更新脚本里重装。
  自检：`bash scripts/verify-cann.sh`（应 `version=9.0.0`、`ccec`、`tikicpulib: OK`、`simulator libs`）。
- `npu` 模式不可用（无实机）。CPU 孪生（`-r cpu`）与 CAModel 仿真（SIM）均可用。

### 非显然的坑（Cloud VM 特有）

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

### 运行/验收（标准命令见各目录，不在此重复）

- 冒烟：`cd ascendc-tests/add_custom && ./run.sh cpu 910B4`、`SIM_DIRECT=1 ./run.sh sim 910B4`
  （`add_custom` 的 SIM 走较老的 camodel 直连路径，本 VM **可正常跑通**）。
- ML-KEM stable 算子（如 PKE Decrypt）：`bash run.sh -r cpu -v Ascend910B4` 开箱即用（CPU 孪生 =
  辅助正确性参考，golden `[verify] PASS max=0`）。

### SIM（stable/exp 新版 aclrtlaunch 路径）的已知阻塞与绕过

- `scripts/sim_env.sh` 会把由 `scripts/stub_libascend_dump.cpp` 编出的 **WSL 用 FPE 桩**
  `libascend_dump.so` 预置到 `out/lib` 并优先加载。该桩缺少
  `toolkit::aicpu::dump::Output::InternalSwap`，在本 **非 WSL** VM 上会让 `libge_common_base.so`
  在运行期报 `symbol lookup error`，`bash run.sh -r sim` 因此失败。
- 本 VM **不会**触发桩要规避的 WSL FPE，真实 `libascend_dump.so`（在 `~/Ascend/cann/lib64`）可正常工作。
  绕过办法（不改仓库代码）：先用 `bash run.sh -r sim ...` 完成一次构建，再把
  `out/lib/libascend_dump.so` 临时移开，让真实库加载后直接跑 `./ascendc_kernels_bbit`
  （`LD_LIBRARY_PATH` 前置 `out/lib:out/lib64:<simulator lib>:~/Ascend/cann/lib64`，并过滤 `*/driver/*`）。
  已如此验证 PKE Decrypt SIM：`Total tick ≈ 283499`、`[verify] PASS max=0`，与文档 ~283k 一致。
- 若要让标准 `run.sh -r sim` 直接可用，需仓库层面改动（如按「是否 WSL」跳过该桩），属代码变更，
  需用户决策，未擅自修改。
