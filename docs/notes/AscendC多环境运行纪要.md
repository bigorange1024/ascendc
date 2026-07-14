# AscendC 多环境运行纪要（WSL / Cloud / 真机）

> **用途**：归纳本仓在三种主机上跑 `run.sh` 时的**环境差异与已知坑**（原理优先，不复述用例细节）。  
> **操作说明**：[NPU真机环境说明.md](../engineering/NPU真机环境说明.md) · [`scripts/runtime_env.sh`](../../scripts/runtime_env.sh) · [`Cursor-Cloud环境说明.md`](../../Cursor-Cloud环境说明.md)  
> **定稿日**：2026-07-14

---

## 1. 不变量

| 不变量 | 说明 |
|--------|------|
| 算子语义不因主机分叉 | 同一 Git 树；正确性由 golden I/O 判定 |
| 真运行时仅三档 | **cpu / sim / npu**（对齐 CMake `RUN_MODE` / CANN） |
| `auto` / `verify` 是编排层 | 本仓脚本便利；**不是**昇腾官方 API；默认可不写（仍为 `cpu`） |
| 探测进共享脚本 | `runtime_env.sh` / `sim_env.sh`；禁止每个用例手写 WSL 特例 |

---

## 2. 三主机模型

```text
WSL（无卡）     → HOST=wsl     → HAS_NPU=0；SIM 可跑；**禁 -r npu**
Cloud/裸 Linux   → HOST=cloud|linux → 通常无卡；SIM 路径与 dump 策略≠WSL
真机 NPU         → HAS_NPU=1   → 允许 -r npu；verify 阶梯可含 npu
```

**选型原则**：需要证明同步/搬运 → **必须以 SIM（或有卡时 npu）证据**；仅 cpu 绿不等于验收完成。

---

## 3. 差异面（按层次）

### 3.1 Host 编译器（正确性前置）

| 现象 | 环境 | 处理 |
|------|------|------|
| `-Wunused-const-variable` + `-Werror` 编译失败 | Cloud 常见 **Clang** | 删未用常量/变量；WSL **GCC 常不报** → 本机绿 ≠ Cloud 绿 |
| `-Werror=comment`（`"/*" within comment`） | 同上；注释写 `input/*.bin` / `output/*.bin` | **禁止**块注释内出现字面量 `/*`；改写为「input 目录下各 .bin」 |
| 其它 `-Werror` 告警升错误 | 同上 | 以 **Cloud 工具链** 为 host 编译权威之一 |

### 3.2 CANN / SIM 动态库

| 现象 | 环境 | 处理 |
|------|------|------|
| dump 桩遮蔽真库 → `InternalSwap` undefined | Cloud（非 WSL） | `sim_env.sh`：**非 WSL 不装** `libascend_dump` 桩；`CAMODEL_SKIP_ADX_WORK_PATH` |
| DumpManager FPE | WSL | WSL **装桩**规避（见 `sim_env.sh` / camodel 日志脚本） |
| 缺 liboqs → golden/KAT 挂 | 新机 / Cloud | `clone-thirdparty.sh`（默认 build）；**勿与 CANN SIM 符号问题混诊** |

### 3.3 `run.sh` 门禁

| 行为 | 说明 |
|------|------|
| 默认 `-r` | **cpu**（兼容旧脚本矩阵） |
| `-r npu` on WSL | `runtime_env` 硬失败 |
| `-r auto` | npu > sim > cpu；落 sim 时默认 `SIM_DIRECT=1` |
| `-r verify` | cpu → `SIM_DIRECT` sim →（有卡）npu；**多进程重入**本脚本 |

声称完成仍须显式 **cpu + SIM**（或 verify）证据；`auto` 单档不够。

---

## 4. Agent / 协作纪律

1. **换机先**：`git pull` → `clone-thirdparty.sh` → `verify-cann.sh`。  
2. **Cloud 跑测**：可当作「第二编译器 + SIM 环境」；绿了回写 STATUS/qa，红了拆 **编译 vs SIM vs 依赖**。  
3. **本机 WSL**：仍是日常开发面；**不要**用假 npu。  
4. **扩 scope**：活跃探针 `run.sh` 已接入 `runtime_env`（2026-07-14）；`frozen/` 不改；T19 device stub（exit 2）跳过。

---

## 5. 附录：近期证据（2026-07-14）

| 项 | 结果 |
|----|------|
| 四例 examples 试点 | Cloud：KEM/encrypt/decrypt 先绿；PKE keygen 因 unused const 挂 → `a5693dc` 修后 CPU+SIM 绿（tick~542k） |
| WSL decrypt verify | cpu+sim PASS |
| 探针批量接入 | `ascendc-tests` 除 2 个 NOT_IMPLEMENTED stub 外，活跃 `run.sh` 已 `source runtime_env.sh` |
