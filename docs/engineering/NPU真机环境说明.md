# NPU 真机环境说明（与 WSL / Cloud 分流）

> **用途**：说明三环境（办公室 WSL、Cursor Cloud / 普通 Linux、昇腾真机）如何选 `-r`，以及真机上板前的冒烟。  
> **运行时探测**：[`scripts/runtime_env.sh`](../../scripts/runtime_env.sh)（由试点 `run.sh` `source`）。  
> **Cloud 专属坑**：[`Cursor-Cloud环境说明.md`](../../Cursor-Cloud环境说明.md)。  
> **WSL 无卡复现**：[`环境复现与开发指南.md`](环境复现与开发指南.md)。

**最后刷新**：2026-07-14（活跃探针 + 4 例 examples 已接入；原理见 [AscendC多环境运行纪要.md](../notes/AscendC多环境运行纪要.md)）。

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

**ascendc-tests 活跃探针**：各目录 `run.sh` 已接入（含 `add_custom`、correctness/`pass-*`）；~~跳过 stub `fix-…-decaps-device`~~ 已废；活跃：[`pass-fix-…-decaps-device-ct-k4`](../../ascendc-tests/pass-fix-f203-alg21-kem-decaps-device-ct-k4/)（本专题）/ main 上无 `-ct` 交付树。Encaps device 已为 [`pass-fix-f203-alg20-kem-encaps-device-k4`](../../ascendc-tests/pass-fix-f203-alg20-kem-encaps-device-k4/)。**不改** `frozen/`。

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
cd examples/stable/stable-fips203-mlkem-pke-decrypt-k4
bash run.sh -r auto -v Ascend910B4     # 有卡 → npu
bash run.sh -r verify -v Ascend910B4   # cpu → sim → npu
```

探测保守条件：`npu-smi info` 成功，且非 WSL；可选存在 `/dev/davinci*`。

---

## 4. 硬禁令

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
