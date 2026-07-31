# 2026-07-30 — 实机 NPU 环境排查方案与 `add_custom` 上板适配清单（仅落文档）

**主题关键字**：借用实机 · `set_env.sh` 在哪 · `scripts/env.sh` 硬编码 · 只读体检清单 · `add_custom` 上板试点 · **本轮不改代码**

---

## 1. 缘起

要把代码搬到昇腾 NPU 实机验证，先拿 [`ascendc-tests/add_custom`](../../ascendc-tests/add_custom/) 上板（AscendC 官方公开示例，本身具备实机能力）；跑通后再照它改自研用例的运行文件，降低试错成本。

用户提出的具体疑问：**借来的实机上 toolkit 与运行环境是怎么装的不清楚**，而 [`scripts/env.sh`](../../scripts/env.sh) 里写的是 `source "${HOME}/ascendc/scripts/env.sh"` 这一套本地假设 —— 该怎么查实机环境、又该改哪里。

## 2. 审计结论（读 `env.sh` / `runtime_env.sh` / `add_custom/{run.sh,CMakeLists.txt,main.cpp}`）

**唯一会「上机即挂」的点是 `scripts/env.sh`**：

- `env.sh:4` 把 `CANN_HOME` 默认成 `${HOME}/Ascend/cann`；
- `env.sh:18` 假定 `set_env.sh` 就在该目录内。

这是办公室 WSL **手工解包**的布局。标准安装（root 装 `/usr/local/Ascend`）里 `set_env.sh` 在 **上一级**：`/usr/local/Ascend/ascend-toolkit/set_env.sh`。

**不必大改的部分**（已经是可移植写法）：

| 组件 | 现状 |
|---|---|
| [`scripts/runtime_env.sh`](../../scripts/runtime_env.sh) | 已按 `CANN_HOME → ASCEND_HOME_PATH → ~/Ascend/cann → ~/Ascend/ascend-toolkit/latest → /usr/local/Ascend/ascend-toolkit/latest` 多根探测；实机无 CAModel 时 `HAS_SIM=0`，`-r verify` 自动退化为 **cpu → npu** |
| `add_custom/scripts/resolve_device.sh` | SoC 已参数化（仅需按实测型号补条目） |
| `add_custom/CMakeLists.txt` | 用的 `${install_path}/{include,lib64,compiler/tikcpp,tools/tikicpulib}` 在标准安装里均存在；只有 **simulator 路径** 带 WSL 包特有的 `toolkit/` 前缀 |

**顺带发现**：[`scripts/verify-cann.sh`](../../scripts/verify-cann.sh) 是纯 WSL 口径 —— 写死 `x86_64-linux`，且 `tikicpulib/Ascend910A`、`simulator/Ascend910A` 用 `test -f`/`test -d` 在 `set -e` 下**硬失败**。实机常为 **aarch64** 且**不装 CAModel**，该脚本会红，但**不代表工具链有问题**。这是上板第一条命令，容易误判，须降级为 WARN。

## 3. 查实机环境的方法（只读三问）

上机第一步只做**只读**探测，回答三个问题：**`set_env.sh` 在哪** / **toolkit 全不全（有无 `ccec`）** / **卡是什么型号**。四组命令与判读规则已写入 [`docs/engineering/NPU真机环境说明.md`](../../docs/engineering/NPU真机环境说明.md) **§3.1**，要点：

- **不猜路径**：`find /usr/local/Ascend "$HOME/Ascend" -maxdepth 3 -name set_env.sh` 定位；`source` 之后由 CANN 自己设的 `ASCEND_HOME_PATH` / `ASCEND_TOOLKIT_HOME` **反推** `CANN_HOME`。
- **区分 toolkit 与 nnrt/nnae**：后者只有推理运行时、**无 `ccec`** ⇒ AscendC 编不了 → **标阻塞**找管理员，不自行安装。
- **借来的机器不装、不改、不 `sudo`**：只用 env 覆盖；`npu-smi info` 的 Process 段看他人是否在用，别抢 0 号卡；设备节点属组不含当前用户即无权 `-r npu`。
- **aarch64 与型号**：`uname -m` 决定凡写死 `x86_64-linux` 处；`npu-smi info` 的 Name 决定 `-v` 取值，不在 `resolve_device.sh` 表内须补条目（**先查再定，不猜**）。

## 4. 改造清单（P0/P1/P2；**本轮未实施**）

完整表格见 NPU真机环境说明 **§4**，摘要：

| 优先级 | 位置 | 改法 |
|---|---|---|
| **P0** | `scripts/env.sh:4,18` | 候选列表探测（含 `ASCEND_TOOLKIT_ENV_FILE` 覆盖），source 后反推 `CANN_HOME`；全未命中**显式报错**不静默 |
| **P0** | `add_custom/run.sh:27` | `${HOME}/ascendc/…` → `${REPO_ROOT}/scripts/env.sh`；该处 `set +e` 会吞掉失败，补 `command -v ccec` 校验 |
| **P0** | `add_custom/main.cpp:39` | `deviceId = 0` → 读 `ASCEND_DEVICE_ID`（多卡机 0 号常被占） |
| **P1** | `scripts/verify-cann.sh:12,30,36` | arch 由 `uname -m` 定；tikicpulib / simulator 缺失降 WARN |
| **P1** | `add_custom/run.sh:213,219` | npu 分支按需补 driver `lib64`；export `ASCEND_DEVICE_ID`；提示文案去 `~/ascendc` |
| **P1** | `resolve_device.sh` | 按实测型号补条目 |
| **P2** | `CMakeLists.txt:46` · `RUN.md` | simulator 路径兼容 `tools/simulator`（仅为实机也能 `-r sim`）；文档补实机步骤 |
| 可选 | 新增 `scripts/probe-npu-host.sh` | 把 §3.1 四组命令封成一次只读体检报告 |

**实施顺序**：§3.1 体检 → 手工 export 验证 `-r cpu`（不碰卡，先证工具链与探测）→ `-r npu` 冒烟 → 再搬到 ML-KEM 用例。

**回归要求**：`env.sh` 是全仓 `run.sh` 的共同入口，改动后**必须**在办公室 WSL 回归 `bash run.sh -r cpu` + `SIM_DIRECT=1 bash run.sh -r sim`，确认候选顺序仍把 `$HOME/Ascend/cann` 排在前、1024/768 全绿。

## 5. 本轮决定

用户选择**只落文档、暂不改代码**：方案写入 NPU真机环境说明 §3.1 + §4，代码改造留待拿到实机体检输出后再动（避免凭猜测改 `env.sh` 反而破坏 WSL 现状）。`qa/TODO.md` 的 **T2-npu** 下挂 **T2-npu-env**（改造清单待实施）。
