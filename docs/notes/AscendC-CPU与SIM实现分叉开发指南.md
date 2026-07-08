# AscendC CPU 与 SIM 实现分叉 — 开发指南

**读者**：后续 Agent / 协作者（未参与本仓库历史讨论）  
**目的**：说明**何时** CPU 孪生与 SIM 必须分叉、**如何**写代码才不误导后人  
**共享头文件**：[`library/shared/ascendc_build_mode.hpp`](../../library/shared/ascendc_build_mode.hpp)  
**案例锚点**：[`pass-fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4`](../../ascendc-tests/pass-fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4/) `main.cpp`（§附录 A）

---

## 1. 原则（先背这三条）

| 优先级 | 规则 |
|--------|------|
| **P-统一** | CPU 与 SIM **能共用同一套算法与 launch 拓扑** → **只保留一套 kernel**；编译期只区分 API（`ICPU_RUN_KF` vs `ACLRT_LAUNCH_KERNEL`）。 |
| **P-分叉** | 因 **平台语义** 必须不同（见 §2）→ **`#if ASCENDC_BUILD_CPU` / `#elif ASCENDC_BUILD_SIM`** 两段互斥 host 代码；**禁止**每用例自建 `F203_FEAS_*` 等 env。 |
| **P-文档** | 每一处分叉写清根因；SIM host 备选拓扑用 **`ASCENDC_SIM_HOST_MODE`** 登记于 §3.3 表（新取值须追加本表，不得新造 env 名）。 |

**验收契约（本仓 Rule）**：声称用例完成须 **CPU + SIM 双模式 golden 一致**；分叉只影响 **host 编排或调试路径**，不改变 I/O 数学。

---

## 2. 何时必须分叉（常见根因）

| 根因 | 现象 | 典型对策 |
|------|------|----------|
| **tikicpu MIX 串行** | AIC 先跑 `CrossCoreWaitFlag`，AIV 尚未 SET → **永久死锁** | CPU：**多 launch**；SIM：可单 launch 长 FSM |
| **GM 写后读可见性** | 同 kernel 标量写 GM，MTE `DataCopy(GM→UB)` 在 SIM 读不到 | 数据**驻留 UB**；GM 仅 dump |
| **SIM session 残留** | Phase-D 后同 session 跑 Phase-E 首错 | `ASCENDC_SIM_HOST_MODE=decaps_2session` 等 |
| **仅算法对照** | CPU 要 gdb 标量环 | kernel 内 `#if ASCENDC_BUILD_CPU` 标量；SIM 向量 |

**不需要分叉**：向量 Barrett mod、DataCopy、Add、MIX MMAD——同一 kernel 源码，编译目标不同。

---

## 3. 全仓统一宏与环境变量（强制）

### 3.1 编译期（两套互斥宏）

CMake `RUN_MODE=cpu` 时定义 **`ASCENDC_CPU_DEBUG`**；所有用例 **include** [`ascendc_build_mode.hpp`](../../library/shared/ascendc_build_mode.hpp) 后使用派生宏：

| 宏 | cpu 构建 | sim/npu 构建 | 用途 |
|----|----------|--------------|------|
| **`ASCENDC_BUILD_CPU`** | `1` | `0` | tikicpu 孪生路径 |
| **`ASCENDC_BUILD_SIM`** | `0` | `1` | CaModel / 设备路径 |

```cpp
#include "ascendc_build_mode.hpp"

#if ASCENDC_BUILD_CPU
// host 或 kernel：CPU 专用
#elif ASCENDC_BUILD_SIM
// host 或 kernel：SIM/NPU 专用
#endif
```

**说明**：`ASCENDC_CPU_DEBUG` 为 CANN/cmake 注入名；**新代码优先写 `ASCENDC_BUILD_CPU` / `ASCENDC_BUILD_SIM`**，与旧 `#ifdef ASCENDC_CPU_DEBUG` 等价。禁止再定义 per-probe 编译宏做平台分叉。

**C++ 辅助**（同头文件 `ascendc::`）：`SimHostModeIs("mode")`、`SimHostEncryptFeasPhasedLaunch()` 等。

### 3.2 Host 编排模板

```text
#include "ascendc_build_mode.hpp"

#if ASCENDC_BUILD_CPU
static int32_t RunCpuXxx(...) { /* 唯一 CPU 拓扑 */ }
#else
static int32_t RunSimDefault(...) { /* SIM 生产默认 */ }
static int32_t RunSimAlt(...)    { /* 仅 SimHostModeIs("xxx") 时 */ }
#endif
```

### 3.3 `ASCENDC_SIM_HOST_MODE` 登记表（全仓唯一 SIM host env）

**仅** `#if ASCENDC_BUILD_SIM` 的 **main/host** 可读；**kernel 内禁止** `getenv` 切 launch。  
未设置 = 各用例文档规定的 **SIM 生产默认**。

| 取值 | 用例 | SIM 行为 | 默认？ |
|------|------|----------|--------|
| *(unset)* | encrypt-compute | 单 launch `f203_encrypt_l18_l19` | ✓ 生产 |
| `phased_launch` | encrypt-compute | 3 launch 调试（同 CPU 拓扑） | 调试 |
| *(unset)* | kem-decaps | 等价 `decaps_2session` | ✓ 生产 |
| `decaps_2session` | kem-decaps | Phase-D 后 `aclFinalize` + fresh session + 设备 FO | 生产 |
| `decaps_1session` | kem-decaps | 单 session（CAModel 易 c′ 污染，排障） | 调试 |

**新增用例**：在本表追加行；`run.sh` 默认 export 生产取值（或 unset）；**禁止**新建 `MY_PROBE_SIM_XXX=1`。

**已废弃**（勿在新代码使用）：`F203_FEAS_FUSED`、`F203_FEAS_SIM_PHASED`、`F203_FEAS_PHASED`；`KEM_DECAPS_SIM_2SESSION`（decaps 仍临时兼容，run.sh 应写 `ASCENDC_SIM_HOST_MODE`）。

### 3.4 与算法 CMake 宏区分

| 类型 | 示例 | 说明 |
|------|------|------|
| **BUILD_CPU / BUILD_SIM** | 上表 | 平台孪生，编译期 |
| **SIM host mode** | `ASCENDC_SIM_HOST_MODE` | host 编排，运行时，SIM only |
| **算法变体** | `F203_STAGE3_MOD`、`ALG11_IMPL` | CPU/SIM **同值**编译，非平台分叉 |

---

## 4. Agent 学习顺序

1. 本指南 §3 + [`ascendc_build_mode.hpp`](../../library/shared/ascendc_build_mode.hpp)  
2. 案例 encrypt-compute `main.cpp`、kem-decaps `main_kem_dec_g5_run.cpp`  
3. MIX FSM / UB 驻留 / SIM session 知识库（§2 链接）  
4. **不要**为 CPU 加 env 试单 launch；**不要**每探针发明新 env 名

---

## 4.1 强制写法（2026-07-08 起，新代码与改 host 编排均须遵守）

凡涉及 **CPU/SIM 平台分叉** 或 **SIM host 备选 launch 拓扑**，按下列三档书写；**禁止**新增第三档之外的 per-probe 运行时 env。

| 档位 | 机制 | 谁读 | 新探针怎么写 |
|------|------|------|--------------|
| **A. 平台** | `#if ASCENDC_BUILD_CPU` / `#elif ASCENDC_BUILD_SIM` | main / host | 编译期互斥两段；CPU 与 SIM 能同拓扑则只分 API（`ICPU_RUN_KF` vs `ACLRT_LAUNCH_KERNEL`） |
| **B. SIM host 拓扑** | `ASCENDC_SIM_HOST_MODE=<登记取值>` + `ascendc::SimHostModeIs(...)` 或专用 helper | **仅** `#if ASCENDC_BUILD_SIM` 的 main | 在 §3.3 **追加一行**；`run.sh` 默认 export 生产取值（或 unset=生产默认） |
| **C. 算法变体** | CMake `CACHE STRING` / `target_compile_definitions` | kernel + host **同值** | 如 `ALG11_IMPL`；**不得**用 env 在 CPU/SIM 间切不同 launch |

**`run.sh` 头注释模板**（强制两段）：

```bash
# Usage（默认）:
#   bash run.sh -r cpu -v Ascend910B4
#   SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
# 调试（非默认）:
#   ASCENDC_SIM_HOST_MODE=<mode> SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

**已废弃、勿在新 `run.sh`/main 使用**（旧脚本可映射到新 env）：`F203_FEAS_*`、`KEM_DECAPS_SIM_2SESSION` 等 per-probe SIM 开关。

**kernel 内禁止** `getenv` 切换 launch 拓扑；所有 session/launch 决策在 **host main** 完成。

---

## 5. 何时写新分叉

满足 **全部** 才新增 `#if ASCENDC_BUILD_CPU` 块，并在 §3.3 **登记表追加一行**（若需 SIM host 备选拓扑）。

---

## 附录 B — kem-decaps SIM 2-session

| 构建 | `ASCENDC_SIM_HOST_MODE` | 行为 |
|------|-------------------------|------|
| **SIM 默认**（unset 或 `decaps_2session`） | 生产 | Phase-D 后 `aclFinalize` + fresh session + 设备 FO |
| **SIM `decaps_1session`** | 调试 | 单 session（CAModel `c′` 污染，排障） |
| **CPU** | （不读） | 单 session，`K max=0` |

Host 判断：`ascendc::SimHostDecapsUse2Session()`（`main_kem_dec_g5_run.cpp`）。  
**已废弃**：`KEM_DECAPS_SIM_2SESSION`（`ascendc_build_mode.hpp` 临时兼容旧脚本）。

---

## 附录 A — encrypt-compute 行 18–19

| 构建 | Host 函数 | Launch |
|------|-----------|--------|
| **CPU** | `RunCpuThreeLaunch` | 3 |
| **SIM 默认** | `RunSimFusedSingleLaunch` | 1 |
| **SIM `phased_launch`** | `RunSimThreeLaunch` | 3 |

内核 `f203_encrypt_l18_l19`：**仅 SIM host 调用**。
