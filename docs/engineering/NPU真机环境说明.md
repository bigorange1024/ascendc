# NPU 真机环境说明（与 WSL / Cloud 分流）

> **用途**：说明三环境（办公室 WSL、Cursor Cloud / 普通 Linux、昇腾真机）如何选 `-r`，以及真机上板前的冒烟。  
> **运行时探测**：[`scripts/runtime_env.sh`](../../scripts/runtime_env.sh)（由试点 `run.sh` `source`）。  
> **Cloud 专属坑**：[`Cursor-Cloud环境说明.md`](../../Cursor-Cloud环境说明.md)。  
> **WSL 无卡复现**：[`环境复现与开发指南.md`](环境复现与开发指南.md)。

**最后刷新**：2026-07-31（§3.2 **二次复核订正**：toolkit/latest 存在且≡cann；tikicpulib **有 910B***；source 后 `CANN_HOME` 为空须回写）。

---

## 1. 三环境对照

| 项 | WSL（办公室 / 家用） | Cloud / 普通 Linux（无卡） | 真机 NPU |
|----|----------------------|----------------------------|----------|
| `ASCENDC_HOST_KIND` | `wsl` | `cloud` 或 `linux` | `linux`（有卡） |
| CPU 孪生 `-r cpu` | ✅ 日常首选 | ✅ | ✅ |
| CAModel `-r sim` | ✅（见 `sim_env.sh` dump 桩） | ✅（非 WSL：不装 dump 桩） | ✅（可选对照） |
| 真机 `-r npu` | ❌ **禁止** | ❌（无卡） | ✅（驱动+CANN 齐） |
| 典型探测 | `HAS_NPU=0` | `HAS_NPU=0`，`HAS_SIM=1` | `HAS_NPU=1` |

代码经 Git 统一；**算子语义不因环境分叉**。差异只在探测脚本与 `run.sh` 门禁。

---

## 2. `run.sh` 模式语义

| 模式 | 行为 |
|------|------|
| **默认 / `-r cpu`** | 仍为 **cpu**（兼容全仓脚本；一期**不**改成 auto） |
| **`-r sim` / `-r npu`** | 显式单档；WSL 上 `-r npu` → `runtime_env` **报错退出** |
| **`-r auto`** | 单档最优：可用 NPU（且用例 `ASCENDC_CASE_SUPPORTS_NPU=1`）→ `npu`；否则有 SIM → `sim`；否则 `cpu`。落 `sim` 时默认 `SIM_DIRECT=1` |
| **`-r verify`** | 验收阶梯：**cpu → `SIM_DIRECT=1` sim**；仅当探测到真机且非 WSL、且用例支持时再跑 **npu**。各档**独立**起本脚本（不混编三种 cmake） |

**说明**：`auto` ≠ 完整验收。Agent **声称完成**仍须有 **cpu + SIM**（及有卡时 npu）证据。`auto`/`verify` 为本仓编排便利，**非**昇腾官方 API。

### 已接入 `runtime_env`

**examples（4 试点）**：KEM KeyGen incubating + PKE keygen/encrypt/decrypt stable。

**ascendc-tests 活跃探针**：各目录 `run.sh` 已接入（含 `add_custom`、correctness/`pass-*`）。KEM Decaps device：**交付** [`pass-fix-…-decaps-device-k4`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg21-kem-decaps-device-k4/) · **CT 专题** [`pass-fix-…-decaps-device-ct-k4`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg21-kem-decaps-device-ct-k4/)。另：KeyGen [`pass-fix-…-keygen-device-k4`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg19-kem-keygen-device-k4/) · Encaps [`…-encaps-device-k4`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg20-kem-encaps-device-k4/)。**不改** `frozen/`。

### 明确不做

- 默认从 `cpu` 改成 `auto`/`verify`
- 批量改 `**/frozen/**` 内 `run.sh`

---

## 3. 真机冒烟（有卡 Linux）

钉版本（与仓库其它文档一致）：

| 组件 | 版本 / 备注 |
|------|-------------|
| CANN | 社区版 **9.0.0**（与 WSL 工具链对齐） |
| 驱动 | 厂商配套当前 CANN；须能 `npu-smi info` |
| SoC | 本仓验收常用 `Ascend910B4`（`-v Ascend910B4`） |

建议顺序：

```bash
npu-smi info                         # 设备可见
bash scripts/verify-cann.sh          # 工具链粗检
cd ascendc-tests/add_custom
bash run.sh -r npu -v Ascend910B4    # 冒烟（若该用例已支持 npu）
```

本仓试点：

```bash
cd examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-decrypt-k4
bash run.sh -r auto -v Ascend910B4     # 有卡 → npu
bash run.sh -r verify -v Ascend910B4   # cpu → sim → npu
```

探测保守条件：`npu-smi info` 成功，且非 WSL；可选存在 `/dev/davinci*`。

> **注意**：`bash scripts/verify-cann.sh` 目前是**面向 WSL 手工解包布局**写的（写死 `x86_64-linux`；`tikicpulib/Ascend910A`、`simulator/Ascend910A` 用 `test -f`/`test -d` 在 `set -e` 下**硬失败**）。实机通常**不装 CAModel**、且可能是 **aarch64**，因此该脚本在实机上很可能红——**不代表工具链有问题**。改造前请以 §3.1 的体检清单为准。

### 3.1 实机只读体检清单（借用机器：不装、不改、不 `sudo`）

上机第一步只做**只读**探测，目的是回答三个问题：**`set_env.sh` 在哪**、**toolkit 全不全（有无 `ccec`）**、**卡是什么型号**。

**第 1 组 — 机器与卡**

```bash
uname -m; uname -r; head -3 /etc/os-release; id
npu-smi info                    # 卡型号(Name)、健康、显存、谁在用
npu-smi info -t board -i 0      # Chip Name / 固件版本
ls -l /dev/davinci* /dev/devmm_svm /dev/hisi_hdc 2>/dev/null   # 设备节点属组 = 有无权限
cat /usr/local/Ascend/driver/version.info 2>/dev/null
```

- `uname -m`：昇腾服务器常为 **aarch64**，凡写死 `x86_64-linux` 的路径都要改。
- `npu-smi info` 的 Name 决定 `-v` 取值；[`resolve_device.sh`](../../ascendc-tests/add_custom/scripts/resolve_device.sh) 现仅覆盖 910A / 910B1–B4 / 310P，**实测型号不在表内须补条目（先查再定，不猜）**。
- 设备节点属组不含当前用户 ⇒ 无权跑 `-r npu`，先找管理员。

**第 2 组 — CANN 装在哪（核心）**

```bash
ls -ld /usr/local/Ascend/* "$HOME/Ascend"/* 2>/dev/null
find /usr/local/Ascend "$HOME/Ascend" -maxdepth 3 -name 'set_env.sh' 2>/dev/null
cat /etc/ascend_install.info 2>/dev/null
cat /usr/local/Ascend/ascend-toolkit/latest/version.info 2>/dev/null
```

`find` 的输出直接决定该 `source` 谁。同时判断装的是 **toolkit**（含编译器，可编 AscendC）还是仅 **nnrt / nnae**（只有推理运行时，**无 `ccec`**）。

**第 3 组 — source 之后工具链齐不齐**

```bash
source <第 2 组找到的 set_env.sh>
echo "ASCEND_HOME_PATH=$ASCEND_HOME_PATH  ASCEND_TOOLKIT_HOME=$ASCEND_TOOLKIT_HOME"
command -v ccec msprof; ccec --version | head -3
ls -d "$ASCEND_HOME_PATH"/{include,lib64,compiler/tikcpp/tikcfw,tools/tikicpulib/lib} 2>&1
ls "$ASCEND_HOME_PATH/tools/tikicpulib/lib" | head        # CPU 孪生有无对应芯片
ls -d "$ASCEND_HOME_PATH"/tools/simulator/* "$ASCEND_HOME_PATH"/toolkit/tools/simulator/* 2>/dev/null
ls "$ASCEND_HOME_PATH/lib64/libruntime.so" "$ASCEND_HOME_PATH/lib64/libascendcl.so"
ldconfig -p | grep -E 'libascend_hal|libruntime|libascendcl' | head
```

- 无 `ccec` ⇒ 只装了运行时，AscendC 编不了 → **标阻塞**，请管理员补 toolkit。
- 无 `libascend_hal` ⇒ driver 的 `lib64` 未进 `ld.so.conf`，上板会报 ACL 107001 类错误，须在 `run.sh` 的 npu 分支补 `LD_LIBRARY_PATH`。
- `simulator/` 缺失是**正常**的（实机一般不装 CAModel）；`runtime_env.sh` 会置 `HAS_SIM=0`，`-r verify` 自动退化成 **cpu → npu**。

**第 4 组 — 构建工具与礼貌检查**

```bash
cmake --version | head -1; gcc --version | head -1; python3 -V
npu-smi info | sed -n '/Process/,$p'     # 有他人进程就别抢 0 号卡
```

**三种典型布局 → `CANN_HOME` 取值**

| 布局 | `source` 谁 | `CANN_HOME` |
|---|---|---|
| root 全量 toolkit（最常见） | `/usr/local/Ascend/ascend-toolkit/set_env.sh` | `/usr/local/Ascend/ascend-toolkit/latest` |
| 用户态装 toolkit | `$HOME/Ascend/ascend-toolkit/set_env.sh` | `$HOME/Ascend/ascend-toolkit/latest` |
| 办公室 WSL（手工解包，本仓现状） | `$HOME/Ascend/cann/set_env.sh` | `$HOME/Ascend/cann` |

**不要猜**：source 完之后由 CANN 自己设好的 `ASCEND_HOME_PATH` / `ASCEND_TOOLKIT_HOME` **反推** `CANN_HOME`。

### 3.2 借入机体检回填（2026-07-31；二次复核已订正）

| 项 | 实测（以二次复核为准） |
|----|------|
| Host | **aarch64** · Ubuntu 22.04 · 账号 root（首次） |
| 卡 | **910B4**（`npu-smi` Name）· `/dev/davinci0`…`davinci7` 存在 · 多卡空闲 |
| CANN 根 | `/usr/local/Ascend/cann` → **`cann-9.1.0-beta.3`**（`readlink -f` 已证）；另有 `cann-8.5.2` |
| `set_env.sh` | `cann/set_env.sh` 与 `cann-9.1.0-beta.3/set_env.sh` **同 inode**；`ascend-toolkit/set_env.sh` → `latest/set_env.sh`（另一入口，落到同一包） |
| `ascend-toolkit/latest` | **存在**：`latest` → `/usr/local/Ascend/cann` → 同上 9.1.0-beta.3（**订正**：首次误报「无 latest」，实为当时 `version.info` 路径不对） |
| source `cann/set_env.sh` 后 | `ASCEND_HOME_PATH`=`ASCEND_TOOLKIT_HOME`=`/usr/local/Ascend/cann-9.1.0-beta.3`；**`CANN_HOME` 为空**；`ASCEND_HOME_DIR` 为空 |
| `ccec` | 路径在 `…/cann-9.1.0-beta.3/bin/ccec`；`--version` 为 **clang 15.0.5**，`Target: aarch64-unknown-linux-gnu`（**不**打印「9.1」字样） |
| `version.info` | 包内/顶层见 **`version=25.5.1`**（与 driver 口径一致；目录名才是 cann-9.1.0-beta.3） |
| 工具链目录 | include / lib64 / `compiler/tikcpp` / `libruntime` / `libascendcl` **齐** |
| `libascend_hal` | 已在 `ldconfig`（driver 路径） |
| tikicpulib | **订正**：存在 **`Ascend910B` / `B1` / `B2` / `B2C` / `B3` / `B4`**（及 910A、910_93xx、310*）。首次 `ls \| head` 截断导致误判「无 910B*」 |

**判读（订正后）**

1. **编译根**：用 `/usr/local/Ascend/cann` 或解析后的 `…/cann-9.1.0-beta.3` 均可；`ascend-toolkit/latest` 与之等价。本仓 `runtime_env` 已探 `…/ascend-toolkit/latest`，**在本机可命中**；仍缺的是默认 **`~/Ascend/cann`**（root 机上通常没有）以及 source 后 **`CANN_HOME` 不被 CANN 脚本设置**。
2. **`env.sh` 必须**：source 成功后用 `ASCEND_HOME_PATH`（或 `ASCEND_TOOLKIT_HOME`）**回写 `CANN_HOME`**；不能假定 `set_env.sh` 会导出 `CANN_HOME`（本机实测为空）。
3. **`-r cpu` 可再试**：本机有 `Ascend910B4` / `Ascend910B1` tikicpulib，与 `resolve_device` 对 910B4 的映射一致。上板仍以 **`-r npu`** 为主；cpu 作无卡逻辑对照，非阻塞。
4. **`-v Ascend910B4`**：与卡名 910B4 匹配，不必改型号表。
5. **版本偏斜仍在**：目录 **9.1.0-beta.3** vs 仓钉 **9.0.0**；`version.info=25.5.1`。`add_custom` 先冒烟，失败再议是否对齐 9.0。
6. **脱敏笔误**：二次贴中 `tools.tikicpulib` 视为 **`tools/tikicpulib`**（斜杠）。

**未改代码时的手工上板**

> **已实施（2026-07-31）**：P0 已合入 — `scripts/env.sh` 多候选 + source 后回写 `CANN_HOME`；`add_custom/run.sh` 用 `REPO_ROOT`；`main.cpp` / `run.sh` 的 **`ASCEND_DEVICE_ID` 缺省为 1**。

```bash
cd <repo>/ascendc-tests/add_custom
# 缺省用逻辑设备 1；若要用 0：ASCEND_DEVICE_ID=0 bash run.sh -r npu -v Ascend910B4
bash run.sh -r npu -v Ascend910B4
# 可选对照：bash run.sh -r cpu -v Ascend910B4
```

---

## 4. 借用实机适配改造清单（待实施；已按 §3.2 二次复核修订）

**背景（2026-07-30 讨论）**：上板试点选 [`ascendc-tests/add_custom`](../../ascendc-tests/add_custom/)。  
**二次复核要点**：`ascend-toolkit/latest` **可用且等价于 cann**；tikicpulib **有 910B***；source 后 **`CANN_HOME` 为空** 须由本仓回写。

| 优先级 | 文件（行） | 现状 | 改法（按 §3.2 二次复核） |
|---|---|---|---|
| **P0** | [`scripts/env.sh`](../../scripts/env.sh) `:4,18` | ~~默认 `~/Ascend/cann`~~ → **已改（2026-07-31）** | 多候选 + `ASCEND_TOOLKIT_ENV_FILE`；source 后用 `ASCEND_HOME_PATH` 回写 `CANN_HOME` |
| **P0** | [`scripts/runtime_env.sh`](../../scripts/runtime_env.sh) | ~~无 `/usr/local/Ascend/cann`~~ → **已加** | 在 `ascend-toolkit/latest` 前插入 `/usr/local/Ascend/cann` |
| **P0** | [`add_custom/main.cpp`](../../ascendc-tests/add_custom/main.cpp) `:39` | ~~`deviceId = 0`~~ → **已改** | 读 `ASCEND_DEVICE_ID`，**缺省 1**（借入多卡约定；`ASCEND_DEVICE_ID=0` 可覆盖） |
| **P0** | [`add_custom/run.sh`](../../ascendc-tests/add_custom/run.sh) `:27` | ~~`${HOME}/ascendc`~~ → **已改** | `${REPO_ROOT}/scripts/env.sh` + `ccec`/`CANN_HOME` 校验；export `ASCEND_DEVICE_ID` 缺省 1 |
| **P1** | [`scripts/verify-cann.sh`](../../scripts/verify-cann.sh) | 写死 `x86_64-linux` | `uname -m` → `aarch64-linux` 等；simulator 缺失 → WARN（本机可不跑 sim） |
| **P1** | `add_custom` CPU | 曾误判无 910B* | **可试** `-r cpu`；失败再记。上板主路径仍是 npu |
| **P1** | `add_custom/run.sh` npu | driver `LD_LIBRARY_PATH` | 本机 ldconfig 已有 hal → 补路径可选；仍 export `ASCEND_DEVICE_ID` |
| **P1** | `resolve_device.sh` | 型号表 | **910B4 已覆盖，不必补** |
| **P2** | CMake simulator / `RUN.md` | — | 纯上板可不动 sim；文档写版本偏斜与 `CANN_HOME` 回写 |
| 风险 | **9.1.0-beta.3** vs 仓钉 **9.0.0** | `version.info=25.5.1` | `add_custom` 冒烟；失败则停，不扩 ML-KEM |

**实施顺序**：二次复核已齐 → 改 P0 → 借入机 **`bash run.sh -r npu -v Ascend910B4`**（可选再跑 cpu）→ 办公室 WSL 回归 cpu+SIM → 再扩 ML-KEM。

**回归要求**：`scripts/env.sh` / `runtime_env.sh` 改动后**必须**在办公室 WSL 回归 `bash run.sh -r cpu` + `SIM_DIRECT=1 bash run.sh -r sim`，确认仍优先命中 `$HOME/Ascend/cann`、1024/768 全绿。

---

## 4.1 实机性能采集：`RUN_WITH_MSPROF=1`（默认关闭）

**教训（2026-07-31 实机首轮）**：`RUN_WITH_MSPROF` 原先**只有 `add_custom/run.sh` 实现**，
ML-KEM 探针一律直跑二进制，实机上设了该变量也**静默无产物**。现已抽出共用包装
[`scripts/msprof_run.sh`](../../scripts/msprof_run.sh)，接入 PKE×3 + KEM×4 探针。

| 命令 | 行为 |
|------|------|
| `bash run.sh -r npu -v Ascend910B4` | **默认**：直跑，等价 `kernel-run-timeout.sh`，**不产** prof/OPPROF |
| `RUN_WITH_MSPROF=1 bash run.sh -r npu -v Ascend910B4` | `msprof op` → `<用例>/prof_npu/<bin>/`（`OPPROF_*` 在其内） |
| `RUN_WITH_MSPROF=1 bash run.sh -r sim -v Ascend910B4` | `msprof op simulator`，很慢；仅需要仿真侧 tick 明细时用 |
| `RUN_WITH_MSPROF=1 … -r cpu` | 告警并退回直跑（CPU 孪生无硬件计数器） |

多 launch 的 device session（KeyGen / Decaps 等）默认只采 `MSPROF_LAUNCH_COUNT_NPU=8` 个
launch，不够时调大；指标集 `MSPROF_AIC_METRICS_NPU` 等其余旋钮见脚本头注释。
真机**不会**打印 CAModel 的 `Total tick`，实机耗时以 msprof 报告为准。

已接入：**PKE×3 + KEM×4 探针**与 **stable 七算子**（`pke-{keygen,encrypt,decrypt}`、
`kem-{keygen,encaps,decaps,decaps-ct}`）。`pke-decrypt` 默认仍走 `/usr/bin/time` 写
`output/run_metrics.txt`（tick 台账依赖），仅 `RUN_WITH_MSPROF=1` 时改走 msprof。

---

## 4.2 实机无 `thirdparty/`：golden 如何出（2026-07-31）

**约束**：借入机不便安装 `thirdparty/`（`liboqs`、私有 `ntt_onnx` 均没有），但要求**每个用例
单独跑都能跑通**。

| 依赖 | 处置 |
|------|------|
| NTT LUT 头（`transpose_mlkem_luts_i8.h`） | 三级回退：`F203_LUT_HDR` → 仓库根私有 `thirdparty/ntt_onnx` → **各交付树内 vendored 副本**（随 git 分发）。见 §10.2 教训 |
| KEM golden（`scripts/liboqs_kem_ref`） | **[`library/shared/f203_kem_ref/kem_ref.py`](../../library/shared/f203_kem_ref/kem_ref.py)**：liboqs 优先；缺失回落仓内已验证 PKE golden + SHA3 组装 KEM 层 |
| Decaps 密钥对 stash（`output/kem_keypair_stash/`，不入 git） | 缺失时按 `SEED_D`（缺省 20260619）derand **现造一对**；固定钥仍可 `EK_KEM_SRC`/`DK_KEM_SRC` |

```bash
KEM_GOLDEN_BACKEND=python bash run.sh -r cpu -v Ascend910B4   # 强制回落自检（非默认）
KEM_GOLDEN_CROSS=1        bash run.sh -r cpu -v Ascend910B4   # liboqs 与回落逐字节互校（非默认）
```

日志会打印 golden 实际来源（`via liboqs` / `via python`），**不允许**在看不到来源的情况下报通过。
**回落不覆盖**：外部 `C_SRC` 且未给 `K_ENC_SRC` 的 Decaps（需完整重加密比对）——此路径本就依赖
liboqs 产密文，脚本会给出可操作报错而非静默假绿。

**WSL 验证方式**：临时移走 `thirdparty/`、`scripts/liboqs_*_ref`、`output/kem_keypair_stash`
后跑全部 14 个用例 CPU，全绿方可认为实机可用。

**已验（2026-07-31，WSL 910B4）**：零 thirdparty CPU **14/14**（KEM 均标 `via python`）；
正常环境 CPU **14/14**（`KEM_GOLDEN_CROSS=1`：KeyGen `ek,dk`、Encaps `c,K` 两路逐字节相同）；
成批 SIM **14/14**、用例根 stray dump 全 0。逐例耗时见当日 qa §11.6。

## 4.3 用例内软链一律相对（2026-07-31）

KEM 用例靠软链复用 PKE 交付树的 `thirdparty/` 与 `scripts/{host_golden,compute}`。原先 `run.sh`
用 `ln -sfn` 写**绝对**路径，仓库路径一换（Cloud `/workspace` ↔ WSL `$HOME/ascendc` ↔ 实机任意
目录）就每跑一次重写一次，`git status` 平白出现改动。1024 的 5 个 `run.sh` 已改
**`ln -sfnr`**，同一份相对链在三种环境通用。

512 / 768 / incubating 尚有 12 处 `ln -sfn` 未改（`qa/TODO.md` **T2-npu-link**）——功能不坏
（每次跑会重写成本机路径），但换机后同样脏工作区。

---

## 5. 硬禁令

| 禁止 | 原因 |
|------|------|
| WSL 上 `-r npu` / 在 verify 中跑 npu | Rule：无实机栈；易假绿或 ACL 错 |
| 无卡 Cloud 上声称 npu 通过 | 一期 `HAS_NPU=0`，不得改脚本绕过 |
| 把默认未写 `-r` 改成 `auto`/`verify`（全仓） | 破坏现有脚本矩阵；一期仅试点加能力 |

覆盖探测（调试，非默认）：

```bash
# 用例显式关掉上板（有卡也让 auto/verify 跳过 npu）
ASCENDC_CASE_SUPPORTS_NPU=0 bash run.sh -r auto -v Ascend910B4
```
