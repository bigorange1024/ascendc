# AscendC CAModel SIM — `func_key` 边界与单 ACL session 约束 — 平台知识库

**读者**：未参与本仓库开发的 AscendC + CAModel SIM 使用者 / Agent  
**目的**：把「SIM 上的算子 `aclrtLaunchKernel` 返回 `507000` 但 NPU/CPU 都没事」从「玄学」拆成两个确定性的平台不变量，以便将来设计 SIM 工程时**第一时间**避免重蹈覆辙。  
**讨论**：[`qa/2026-06/2026-06-30-funckey-507000本地独立验证.md`](../../qa/2026-06/2026-06-30-funckey-507000本地独立验证.md)（§1–§9 含错误尝试与受控实验）  
**案例锚点**：[`../../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-encrypt-k4/`](../../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-encrypt-k4/)（Alg.14 Encrypt at_r5 合并核重构，§6 附录）  
**也适用于**：任何 SIM 上 `aclrtLaunchKernel` 返回 `507000 ACL_ERROR_RT_INTERNAL_ERROR` 而 CPU 标量孪生 PASS 的探针

---

## 0. 本文怎么读

| 章节 | 内容 | 是否依赖某次探针的代号 |
|------|------|----------------------|
| §1 | 现象与边界（错误码、可观测特征） | 否 |
| §2 | 工程不变量（两条病根） | 否 |
| §3 | 平台落地模型（`func_key` 分配、AIV-only vs MIX、`merge_aiv_obj`） | 少量（CAModel 9.0） |
| §4 | 验证与诊断方法论（`nm`、受控 `KERNEL_FILES` 实验、双模式对照） | 否 |
| §5 | 可复用模式 P1–P6 | 否 |
| §6 | 案例附录：F203 Encrypt at_r5 落地过程 | 是 |

---

## 1. 现象与边界

### 1.1 错误码

```
[runtime] aclrtLaunchKernel ret=507000 (ACL_ERROR_RT_INTERNAL_ERROR)
```

只在 **CAModel SIM** 出现；同一探针在 **CPU 孪生（`ICPU_RUN_KF`）** 与真机 **NPU** 都正常。错误码不是数值/内存错误，是 **runtime 装载与调度阶段的拒绝**。

### 1.2 病根（两条独立必要条件）

| # | 病根 | 触发面 | 失败表象 |
|---|------|--------|----------|
| **R1** | CAModel 单个 SIM binary 内 **AIV-only kernel `func_key ≥ 5`** 一律 launch 返回 `507000` | 编译期（`KERNEL_FILES` 集合 + ascendc 的 `func_key` 分配） | 某个 device kernel 被 `aclrtLaunchKernel` 投递时立即返回非零；其他 `func_key ≤ 4` 的同类核完全正常 |
| **R2** | host 在 D2H 读 device 写出的 GM 之前**未调用** `aclrtSynchronizeStream`；或一次 PKE 内**多次 `aclInit + aclrtSetDevice + aclFinalize`** | 运行期（host 编排） | D2H 数据全 0 / 旧值；或后续 launch 莫名 `507000`；或 SIM 末尾 `free(): invalid pointer` |
| **R3** | 一个 ACL session 内链接/加载**多个设备 `.so`**（多个 `ascendc_library`），其 `func_key` 空间重叠 | 编译期（多 `ascendc_library` 目标）+ 运行期（同 session 跨库 launch） | **无错误码**：先加载库「活跃」，后加载库的核 launch 被派发到错误 binary → 输出**形状对值全错**（本仓 Alg.21 Decaps 重加密 `c' max=244`）；fresh session 只 launch 一侧则恢复。**修法：合并单设备库**（详见 [`F203-KEM-Alg21-Decaps设备全链与SIM单session技术总结.md`](F203-KEM-Alg21-Decaps设备全链与SIM单session技术总结.md) §4.3） |

`func_key ≤ 4` **并非** ascendc 文档承诺的稳定 ABI；它是 **CAModel 9.0 + 我们当前 SoC（Ascend910B4）** 的经验下界。文档没明确该上限，故应作为**工程硬约束**对待，而不是 best-practice。

### 1.3 与 NPU/CPU 的差异原因

| 平台 | `func_key` 边界？ | 同步流要求？ | 备注 |
|------|------------------|-------------|------|
| CPU (`ICPU_RUN_KF`) | 否（没有 binary 概念） | 否（同步语义） | 单线程顺序，所有 R1/R2 病根天然不触发 |
| NPU | 边界更宽（未在本仓独立验证） | 是 | 真机不在本仓研究范围；CPU PASS 不能推到 NPU |
| SIM (CAModel) | **是，`≤ 4`** | **是** | 本文唯一负责的平台 |

**经验**：如果只在 SIM 上失败而 CPU PASS，**先查 R1/R2，再查算法**。

---

## 2. 工程不变量

### 2.1 不变量 I1（AIV-only 核数预算）

> **一份 SIM device binary 中，AIV-only kernel 数量必须 ≤ 5。** 超过时，第 6 个起的核**无论谁去 `aclrtLaunchKernel`** 都返回 `507000`，与该核的算法/输入/形状无关。

推论：
- **核数 = 资源**：与 UB 字节、stream 数同级别的资源；早期设计就要分配。
- **AIV-only 与 MIX 都消耗 `func_key`**，但 **MIX_AIC_x_y 的核**目前观察到不被边界 R1 拦截（推测因为它带 AIC，被分到不同的 binary section 或不同的 key 空间）。占位为 MIX 是一种合法绕开手段。
- **`KERNEL_FILES` 是工程门面**：哪怕某个 `.cpp` 里定义了未来才会用到的 kernel，只要它出现在 `KERNEL_FILES`，就会吃掉一个 `func_key` 名额。

### 2.2 不变量 I2（host–device 同步）

> **任何 host 端读取 device 写出 GM 的 `aclrtMemcpy(... DEVICE_TO_HOST)`，前面必须有 `aclrtSynchronizeStream`。** 任何「一次完整推理」内的 `aclInit / aclrtSetDevice / aclrtCreateStream / aclFinalize` 都应**只发生一次**（单 ACL session）。

推论：
- 不要把 stream 同步当作「best practice」而是**硬要求**。多次开关 ACL session 在 SIM 下会清空 device binary 缓存触发 ACL 重新加载，本身又是 R1 触发面。
- **「装一次 device 数据 → kernel A → kernel B → 一次性 D2H」** 是 SIM 上最稳的 host 编排骨架。
- 中间需要 host 看 device 写出值（如本案例中 `matM` 的 host 拼装），**必须显式 `aclrtSynchronizeStream`**，不能依赖「launch 已经返回」的错觉。

### 2.3 设计期推论：合并核优先于拆细核

R1 把「AIV-only 核数」打成稀缺资源。设计 SIM 算子时：

| 做法 | 推论 |
|------|------|
| 把同一阶段、共享输入或共享中间量的多个 launch **合并成单 launch（多输出）** | 减一个 `func_key`，且少一次同步 |
| 把「同形状 / 同布局 / 仅维度数不同」的 kernel **参数化为同一份代码 + tiling const** | 多 SoC 之间也省 binary 体积 |
| 在 host 端**便宜计算 / 拼装 / 解包**，把 device 留给真正的 vector/cube 算力 | host 拼装 `matM` 等是 SIM 时代的合法策略 |
| 把上游/下游里**只是数据通路**的 AIV-only 占位核改为 **MIX_AIC_1_2 占位** | 让出 AIV `func_key` 名额；只要 AIC 段空跑或非常轻就够 |

---

## 3. 平台落地模型（AscendC + CAModel SIM）

### 3.1 `func_key` 是什么、从哪来

- `func_key` 由 ascendc 编译期为每个 `__global__ __aicore__ void` 入口分配，**不按源码出现顺序**，也**不按文件加入 `KERNEL_FILES` 的顺序**。
- 对同一 `KERNEL_FILES` 集合多次 build，分配顺序**稳定**（可重现）；但**增删任一文件**会重新分配。
- 可用 `nm build/<...>/device_aiv.o` 读取符号表确认每个 kernel 的 `func_key` 落点。

### 3.2 AIV-only / MIX 在 binary 中的位置

| KERNEL_TASK_TYPE | 是否走 AIV func_key | R1 是否拦截 |
|------------------|--------------------|------------|
| `KERNEL_TYPE_AIV_ONLY`（`SetCoreType("AIV")` + 全 UB） | 是 | **拦截**（≤4 通过，≥5 一律 507000） |
| `KERNEL_TYPE_MIX_AIC_1_2`（含 AIC 段，即使 AIC 几乎空跑） | 否（落 AIC `func_key` 空间） | 当前观察**不拦截**；可作占位绕开 |

实务上：让 `decode_t_hat` / `pack` 这类**纯数据通路** AIV-only 核改成「AIC 空 + AIV 原逻辑」的 MIX 占位，把 AIV `func_key` 名额留给真正的算力核（NTT/INTT/at_r5/g4_noise）。

### 3.3 KernelLaunch 单 session 骨架

```cpp
aclInit(nullptr);
aclrtSetDevice(0);
aclrtCreateStream(&stream);

aclrtMalloc(...);             // 一次 H2D 把 ek/coins/coords/luts 全部上设备
aclrtMemcpyAsync(... stream); // 注意 Async + stream

ACLRT_LAUNCH_KERNEL(prep_a_hat)(blockDim, stream, ...);
ACLRT_LAUNCH_KERNEL(prep_re)(...);
ACLRT_LAUNCH_KERNEL(ntt_r)(...);
ACLRT_LAUNCH_KERNEL(decode_t_hat)(...);

aclrtSynchronizeStream(stream);     // ★ host 要读 device 写出 GM 前必须
aclrtMemcpy(matHost, ..., DEVICE_TO_HOST);
host_pack_matM(matHost, ...);
aclrtMemcpy(matDev, matHost, ..., HOST_TO_DEVICE);

ACLRT_LAUNCH_KERNEL(at_r5)(blockDim, stream, matDev, rDev, uTrDev);

aclrtSynchronizeStream(stream);
aclrtMemcpy(uTrHost, uTrDev, ..., DEVICE_TO_HOST);

aclrtFree(...); aclrtDestroyStream(stream);
aclrtResetDevice(0); aclFinalize();
```

不要：
- 在「prep 段」和「G3 段」之间 `aclFinalize` + 再 `aclInit`；
- 在不需要 host 介入时仍走 `aclrtMemcpy` 三明治（device → host → device）；
- 把 `aclrtSynchronizeStream` 当作「调试时多加一行没坏处」的可选项 — 它是契约，不是装饰。

---

## 4. 验证与诊断方法论

### 4.1 怀疑 R1 时的标准诊断流程

1. **看 `nm` 给的 `func_key`**：
   ```bash
   nm build/CMakeFiles/ascendc_kernels_sim_aiv_device_dir/device_aiv.o \
     | grep -E "f203_encrypt_[a-z0-9_]+_funckey" | sort
   ```
   找出失败 kernel 的 `func_key`；若 ≥ 5，R1 已基本坐实。
2. **受控收缩 `KERNEL_FILES`**：临时在 CMake 加一个 cache 开关（本仓示范 `F203_FUNCKEY_EXPERIMENT`），ON 时把无关 AIV-only 核**从 `KERNEL_FILES` 删除**，让目标 kernel 落到 `func_key ≤ 4`。**注意**：删除文件本身没动产物代码、不动 host main；只是把它从 SIM device binary 里去掉。
3. **跑同一 host 单点 launch**：观察 ret 是否从 `507000` 变成 `0`。变成 `0` 即 R1 病根成立。
4. **永久守卫**：把该 CMake 开关用 `option(... OFF)` 保留为永久证据，注释清楚为什么。

### 4.2 怀疑 R2 时的标准检查

- host main 中 `aclInit / aclFinalize` 次数 = `aclrtSetDevice / aclrtResetDevice` 次数 = `1`。多次出现就重构成单 session。
- 每个 `aclrtMemcpy(..., DEVICE_TO_HOST)` 前面追溯：最近一次 `aclrtLaunchKernel` 到此处之间，是否有 `aclrtSynchronizeStream(stream)`。
- `valgrind` / `ASAN` 在 SIM 上不一定能跑；用「打印中间 buffer 校验和」更稳。

### 4.3 双模式对照永远要 PASS

- **CPU 全 PASS、SIM 失败** → 多半 R1/R2，先不要怀疑算法。
- **CPU 失败、SIM PASS** → 多半 host 拼装/golden 一边的 bug，与 R1/R2 无关。
- **CPU 失败、SIM 失败** → 先修 CPU；CPU 不通就别碰 SIM。

### 4.4 性能数据的正确读法

- **wall_sec 是「冷启动 + build + Model + verify + python cmp」**，**不是 kernel 时间**。SIM build 单次 5–6 分钟很正常。
- **`Total tick` 才是 kernel 在 CAModel 上的「设备拍数」**，跨 binary 也可比；与「时钟」按 SoC period 反推。
- 比较优化前后 tick 必须**在同一 SoC + 同一 `KERNEL_FILES`**，否则 binary 体积差异会带来 cache miss 噪音。

---

## 5. 可复用模式（P1–P6）

> 把现象 / 不变量 / 做法 / 禁忌四件套写齐，方便未来探针对照。

### P1 — 「最先想合并、再想拆」

| 项 | 内容 |
|----|------|
| **问题** | 一个算法阶段（例如 NTT 域线性层）习惯按子任务拆 4 个 AIV-only 核 |
| **不变量** | I1（AIV-only ≤ 5） |
| **做法** | host 端 reshape/repack 输入，把 `[K]`/`[K]` → `[K, P]`（`P=K+辅 poly 数`），用一个核做 `out[p] = Σ_k M[k,p] · r[k]` |
| **禁忌** | 为「核更简单」拆得更细而无视核数预算 |

### P2 — 「单 ACL session 是默认，不是优化」

| 项 | 内容 |
|----|------|
| **问题** | 习惯把每个阶段独立写成 `run_*_device_once(input, output)` 内部带 `aclInit/aclFinalize` |
| **不变量** | I2（单 session）|
| **做法** | host 顶层一次 `aclInit + Set + CreateStream + Finalize`；中间阶段只是函数调用，stream 透传 |
| **禁忌** | 「调试时方便」就在子函数里另开 session — 会埋下 R1 触发面（重载 binary）|

### P3 — 「host 拼装是 SIM 时代的合法策略」

| 项 | 内容 |
|----|------|
| **问题** | device 端布局不直接匹配下游 kernel 输入（如 `Â` 行列对调、需要拼接辅 poly） |
| **不变量** | I1 + I2 |
| **做法** | 在 host 用 `std::memcpy` 拼装；前置 `aclrtSynchronizeStream` 保证读到 device 已写完的 GM；之后 H2D 一次到 device |
| **禁忌** | 为追求「全 device」硬塞一个 AIV-only 重排核，挤掉 `func_key` 名额 |

### P4 — 「AIV-only 占位核改 MIX 占位」

| 项 | 内容 |
|----|------|
| **问题** | 「数据通路」型 AIV-only kernel（decode/pack/打散）吃 `func_key` |
| **不变量** | I1 |
| **做法** | 把同样的 AIV 逻辑挂进 `KERNEL_TYPE_MIX_AIC_1_2`，AIC 段几乎空跑即可；该核不再吃 AIV `func_key` |
| **禁忌** | 改 MIX 后不重新 `nm` 验 `func_key`（信任而不验证）|

### P5 — 「受控 CMake 开关 + 永久守卫」

| 项 | 内容 |
|----|------|
| **问题** | 怀疑某 kernel 是 funckey 病根，需要受控实验，但又怕实验代码长期留在主路径 |
| **不变量** | — |
| **做法** | 加 `option(<NAME>_EXPERIMENT OFF)`，ON 时 `list(REMOVE_ITEM KERNEL_FILES ...)`；main 中以 `#if defined(<NAME>_EXPERIMENT)` 套受控分支；默认 OFF 0 影响生产，但保留作历史证据，避免后人重复试错 |
| **禁忌** | 把实验代码混进生产分支或事后删干净 — 一年后再次遇到 R1 时没人记得验过 |

### P6 — 「双模式做契约，不做选择」

| 项 | 内容 |
|----|------|
| **问题** | 想偷懒只跑 CPU 或只跑 SIM 之一 |
| **不变量** | R1/R2 仅 SIM 触发；算法 bug 双方都触发 |
| **做法** | 任何「声称完成」前 `bash run.sh -r cpu` 与 `bash run.sh -r sim` 都 PASS；`ENCRYPT_VERIFY=1` 双 max=0 |
| **禁忌** | 只看 SIM `c.bin` 大小对就声称 PASS（参考 [ascendc-development.mdc](../../.cursor/rules/ascendc-development.mdc) §「Agent 跑用例验收」）|

---

## 6. 附录：F203 Encrypt at_r5 案例

> 本节是**实例**，印证 §1–§5；换参数（k, N, poly 数）时以原理章节为准。

### 6.1 起点（错误路径）

`stable-fips203-mlkem-pke-encrypt-k4` 的 G3（线性层）一度长成这样：

| kernel | 算什么 | KERNEL_TASK_TYPE | SIM 表现 |
|--------|--------|------------------|----------|
| `f203_encrypt_g3_linear` | `[Âᵀ·r̂ \| t̂·r̂]` 五参合并 | AIV_ONLY | `aclrtLaunchKernel` **507000** |
| `f203_encrypt_g3_linear4` | 同上四 GM | AIV_ONLY | **507000** |
| `f203_encrypt_at_r` | `Âᵀ·r̂ → û` | AIV_ONLY | ✅（独立 session 才行）|
| `f203_encrypt_t_dot_r` | `t̂·r̂ → tr̂` | AIV_ONLY | **507000** |

当时（2026-06-23）的判读（详 [`G3_SIM_AUDIT.md`](../../ascendc-tests/frozen/frozen-fix-f203-alg14-pke-encrypt-correctness-k4/G3_SIM_AUDIT.md) §3.1、§9.2、§10）：
- 错误归因 A：「`f203_encrypt_t_dot_r` 入口在 SIM 上无法完成 launch 注册/调度」 → 用 `at_r(t̂_col0)` 等价绕开，作为「t_dot_r 入口 bug」。
- 错误归因 B：把「`g3_linear` 五参 launch 失败」归到「单 TPipe / 五参 ABI 不兼容」。
- 工程权宜：用两次 `at_r`（`run_g3_at_r_device_once` + `run_g3_tr_via_at_r_device_once`），**每次独立 `aclInit/aclFinalize`**，绕过 `t_dot_r` 与 `g3_linear`。这恰好同时踩中 R1（`func_key=8` 的 `at_r` 在多 binary 重载后偶尔 launch 成功，但只是侥幸）与 R2（多 session）两条病根。
- 进一步：G4 段 `g4_noise` / `pack` 也 507000，被迫降级为 **host 标量** G4 tail（`f203_encrypt_g4_host_scalar.hpp` / `f203_encrypt_pack_host_scalar.hpp`）。

事后回看：当时把所有失败都解释成「kernel 入口本身有问题」，**没有看到「binary 内 AIV 核数超 5」这个共因**。

### 6.2 转折（家里 agent commit `27cc93b` 提出假说）

家里 agent 在新探针 `fix-f203-alg14-encrypt-2launch-k4` 用对照实验给出 R1 命题：「单 binary 内 AIV-only `func_key ≥ 5` → 507000」。本地暂未 pull，转而在原探针**独立证伪**：

1. 加 `option(F203_FUNCKEY_EXPERIMENT OFF)`；ON 时把 `at_r5` / `g4_add_e1` / `g4_make_v` / `pack` 从 `KERNEL_FILES` 移除，AIV-only 核压到 5 个（`marker / prep_a_hat / prep_re / decode_t_hat / g4_noise`）。
2. 同一份 `g4_noise` kernel：原 build `func_key=8` → 507000；实验 build `func_key=4` → **ret=0 SUCCESS**。
3. 这是「同一份代码 / 同一 host / 同一 SoC，只改 `KERNEL_FILES` 就在 R1 边界两侧切换」的**正向证伪**，R1 命题坐实。

详 [qa/2026-06-30 §2–§5](../../qa/2026-06/2026-06-30-funckey-507000本地独立验证.md#2-本地间接证据nm--历史现象)。

### 6.3 落地（at_r5 合并核 + 单 session）

| 改动 | §对应模式 |
|------|----------|
| 新建 `at_r5` kernel：`kP=5`，单 launch 出 `[û[0..3] \| tr̂]` | P1（最先想合并） |
| host 拼 `matM[(j*kP+p)*N+n]`：`p<4` 取 `Â[j,p]`、`p=4` 取 `t̂[j]` | P3（host 拼装） |
| `run_g5_sim_phase1` 一次 `aclInit`，prep+NTT+decode+at_r5 在同一 `stream` | P2（单 session） |
| host 读 `aHat/tHat` 前与 D2H 读 `uTrDev` 前各一次 `aclrtSynchronizeStream` | I2 |
| 从 `KERNEL_FILES` 永久移除 `g3_linear.cpp`（连带 g3_linear/g3_linear4/at_r/t_dot_r 4 个 kernel）；文件留 `compute/g3_linear/` 作历史证据 | P5（守卫）+ 不变量 I1 |
| ROM 表（`alg11_rom_tables.cpp`）通过宏重命名 `gAlg11*Gm → gAtR5*Gm` 后 unity-include，规避 SIM `merge_aiv_obj` 要求每个 .o 自带 GM 符号 + CPU 多重定义之间的两难 | 与本知识库无关，但记一笔以备链接器层面排查 |

### 6.4 实现效果（双模式）

**SIM 测试通过声明**（2026-06-30 12:58 UTC+8）：探针 `../../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-encrypt-k4/`、命令 `ENCRYPT_VERIFY=1 bash run.sh -r sim -v Ascend910B4`、退出码 0、`[SUCCESS] ... gate=G5 (sim) ENCRYPT_VERIFY=1`、`Total tick=43479`、`aclrtLaunchKernel` 返回 `507000` 次数 = 0。详 [`STATUS.md`](../../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-encrypt-k4/STATUS.md) §SIM 测试通过声明。

| 项 | CPU | SIM | 来源 |
|---|---|---|---|
| `bash run.sh -r <mode> -v Ascend910B4` `gate_g1/g2/g3` | ✅ | ✅ | `verify_gate.py` |
| `ENCRYPT_VERIFY=1` c.bin vs golden_c.bin | `[verify] PASS max=0 (1568 bytes)` | `[verify] PASS max=0 (1568 bytes)` | `scripts/verify_encrypt.py`（外壳）|
| `aclrtLaunchKernel` 返回 `507000` 次数 | n/a | **0** | run.sh 输出 |
| host binary `nm` 残留 `g3_linear/at_r/t_dot_r` | n/a | 空 | `nm out/bin/ascendc_kernels_bbit` |
| 装到 SIM include 树的 `aclrtlaunch_f203_encrypt_*.h` | n/a | 11 个活跃核 | `out/include/ascendc_kernels_sim/` |

### 6.5 性能数据（SIM CAModel）

> 数据来源：2026-06-30 跑 `ENCRYPT_VERIFY=1 bash run.sh -r sim -v Ascend910B4`，单点取样；wall_sec 含 build。

| 指标 | at_r5 落地后 | 旧路径（at_r 两 session + g4_noise host fallback） | 备注 |
|------|---------------|--------------------------------------------------|------|
| `Model RUN TIME`（CAModel kernel-only） | **23415.7 ms** | 21500 ms（旧 G3_SIM_AUDIT §11 估计） | 多了 `at_r5 kP=5` 的一行 → 但少了第二次 ACL session 装载 |
| `Total tick`（CAModel）| **43479** | at_r 单次 ~43800（§G3_SIM_AUDIT 4.3.2.4）+ 第二次 ~43800 → 合计 ~87600 | **~50% 节省**：合并核单 launch 取代两次 at_r |
| `wall_sec`（含 build + verify + python cmp）| 334.30 s | 367 s（§STATUS 旧表）| build 部分相近，节省主要在跳过第二次 ACL session 启动 |
| AIV-only kernel 数（device binary）| **9**（marker / prep_a_hat / prep_re / ntt_r / decode_t_hat / at_r5 / intt / g4_noise / 共享 SHAKE 内嵌）| 12（含 g3_linear / g3_linear4 / at_r / t_dot_r 4 个旧核）| `func_key` 名额从「踩到 ≥5 边界」→「at_r5 ≤4」|

**性能写作准则**：tick 是首要指标；wall_sec 仅作工程感知。本案例 tick 减半是因为 launch 合并，不是 vector 路径变快 — 不要把它包装成「kernel 优化」。

### 6.6 经验 / 反模式 ↔ 原理映射表

| 当时的错误判读 | 错在哪 | 对应原理 |
|---------------|-------|----------|
| 「`f203_encrypt_t_dot_r` 入口 SIM 不兼容，绕开就好」 | 没看 `func_key` 表，把「核序问题」当「核本身问题」 | I1 / §4.1 诊断 |
| 「在 G3 段独立 `aclInit/aclFinalize` 是 CANN 9.0 SIM 必须」 | 多 session 反而是 R2 触发面 | I2 / P2 |
| 「g4_noise / pack 必须改 host 标量，因 SIM 不支持这两个核」 | 它们没坏，只是 `func_key=8/9/10/11` 全踩 R1 边界 | I1 / P4 |
| 「`F203_FUNCKEY_EXPERIMENT` 是临时调试代码，PASS 后该删」 | 删了下次又得重做半天受控实验 | P5（永久守卫） |
| 「SIM `c.bin` 字节对了 = 通过验收」 | 只跑 SIM 没跑 CPU 孪生，算法 bug 会漏 | P6（双模式契约） |

### 6.7 代码与文档索引

| 角色 | 路径 |
|------|------|
| 新 G3 合并核 | [`../../ascendc-tests/.../compute/at_r5/f203_encrypt_at_r5_kernel.cpp`](../../ascendc-tests/frozen/frozen-fix-f203-alg14-pke-encrypt-correctness-k4/compute/at_r5/f203_encrypt_at_r5_kernel.cpp) |
| `at_r5` tiling / layout / CPU scalar | 同目录 `f203_encrypt_at_r5_{tiling,layout}.h` / `_ub_scalar.hpp` |
| 单 session host 编排（SIM） | [`main_encrypt_g5_run.cpp` `run_g5_sim_phase1`](../../ascendc-tests/frozen/frozen-fix-f203-alg14-pke-encrypt-correctness-k4/main_encrypt_g5_run.cpp) |
| 单 session host 编排（CPU 孪生） | 同文件 `run_encrypt_g5_cpu_full` |
| CMake funckey 守卫 | [`CMakeLists.txt`](../../ascendc-tests/frozen/frozen-fix-f203-alg14-pke-encrypt-correctness-k4/CMakeLists.txt) 关键字 `F203_FUNCKEY_EXPERIMENT` |
| 旧错误路径（保留作历史证据，不参编） | [`compute/g3_linear/`](../../ascendc-tests/frozen/frozen-fix-f203-alg14-pke-encrypt-correctness-k4/compute/g3_linear/)、[`compute/at_r/`](../../ascendc-tests/frozen/frozen-fix-f203-alg14-pke-encrypt-correctness-k4/compute/at_r/)、[`compute/t_dot_r/`](../../ascendc-tests/frozen/frozen-fix-f203-alg14-pke-encrypt-correctness-k4/compute/t_dot_r/) |
| 历史 SIM 审计（误诊原文 + 修正注） | [`G3_SIM_AUDIT.md`](../../ascendc-tests/frozen/frozen-fix-f203-alg14-pke-encrypt-correctness-k4/G3_SIM_AUDIT.md) §12（修正） |
| 受控实验纪要 | [`qa/2026-06/2026-06-30-funckey-507000本地独立验证.md`](../../qa/2026-06/2026-06-30-funckey-507000本地独立验证.md) |

---

## 7. 相关文档

- [`docs/notes/ascendc-DataCopy与数据搬运知识库.md`](ascendc-DataCopy与数据搬运知识库.md)（MTE 与搬运契约）
- [`docs/notes/ascendc-TQue与Pipe框架知识库.md`](ascendc-TQue与Pipe框架知识库.md)（TPipe/event）
- [`docs/engineering/用例自包含与设备全链约束.md`](../engineering/用例自包含与设备全链约束.md)
- [`.cursor/rules/ascendc-development.mdc`](../../.cursor/rules/ascendc-development.mdc)「Agent 跑用例验收」段：CPU + SIM 双模式契约
- 案例 [`../../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-encrypt-k4/`](../../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-encrypt-k4/)：`STATUS.md` / `INTEGRATION_PLAN.md` §2.3、§4

---

## 维护

- 新的 SIM 病例若不落在 R1/R2 之外的第三种病根上，应在 §2 增列 R3 并到 §5 增列对应 P；
- 新的反例（如某次 `func_key=6` 也 launch 成功）应记 §6 类似案例并修订 §1.2 「`≤ 4`」上限；
- 本文件不属于任何单一探针，迁移/合并 KeyGen / Decrypt 等用例时**应直接引用**，不得复制粘贴。
