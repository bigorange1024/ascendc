# G3 SIM 审计记录（Alg.14 Encrypt 探针）

**探针**：`ascendc-tests/fix-f203-alg14-pke-encrypt-correctness-k4/`  
**关联**：[`INTEGRATION_PLAN.md`](INTEGRATION_PLAN.md) · [`STATUS.md`](STATUS.md)  
**讨论**：`qa/2026-06/2026-06-23-SampleNTT-PhaseA向量化讨论.md` §Alg.14 G3、[`qa/2026-06/2026-06-30-funckey-507000本地独立验证.md`](../../qa/2026-06/2026-06-30-funckey-507000本地独立验证.md)

> ⚠️ **2026-06-30 修正注（先读 §12）**：本文 §1–§11 含**已证伪的错误归因**（把 `func_key ≥ 5` 边界与单 ACL session 未同步问题误判为「`t_dot_r` 入口失效」「五参 ABI 不兼容」「`g4_noise/pack` SIM 不支持」等）。原文保留作历史证据与「同类反模式」教学样本；当前实现以 §12 + [`INTEGRATION_PLAN.md`](INTEGRATION_PLAN.md) §2.3 为准。原理沉淀见 [`docs/notes/AscendC-CAModel-SIM-funckey与单session约束知识库.md`](../../docs/notes/AscendC-CAModel-SIM-funckey与单session约束知识库.md)。

本文**原封不动**保留 G3 阶段曾出现的事实与 workaround 历史；修正完成后在 §6 / §12 更新「已修复」项，**不删除** §1–§11 原文。

---

## 1. G3 语义（FIPS 203 Alg.14 线性层，NTT 域）

| 输出 | 形状 | 计算 |
|------|------|------|
| `u_hat` | `[K,256]` int32 | `Âᵀ · r̂`（`a_hat` 为 `[K,K,256]`，索引 `(j,p)` 对调于时域 Â） |
| `tr_hat` | `[256]` int32 | `t̂ · r̂`（`t_hat` 为 `[K,256]`，与 `r_hat` 内积后得单 poly） |

Golden：`scripts/host_golden/gate_g3.py`（独立 Python，非设备逻辑复刻）。

---

## 2. CPU 路径（自始正确）

- **单次** `f203_encrypt_g3_linear(a_hat, r_hat, t_hat → u_hat, tr_hat)`。
- 核实现：`compute/g3_linear/f203_encrypt_g3_linear.cpp` — 单 `TPipe`，`ProcessFull()` = L4 `ProcessAtR()` + L5 `ProcessTDotR()`。
- CPU 对拍：`ENCRYPT_GATE=3 bash run.sh -r cpu` → `verify_gate.py` G3 **max=0**。

---

## 3. SIM 路径历史问题（修正前）

### 3.1 CPU vs SIM 实现不对等

| 项 | CPU | SIM（修正前） |
|----|-----|----------------|
| G3 launch | 1× `f203_encrypt_g3_linear` | 2× `f203_encrypt_at_r`（`run_at_r_device_once`） |
| `u_hat` | 真 `a_hat` + `r_hat` | 真 `a_hat` + `r_hat`（第一次 at_r） |
| `tr_hat` | 真 `t_hat` + `r_hat`（g3_linear L5） | **fake-Â**：Host `pack_t_hat_fake_a_hat` 把 `t_hat[j]` 填入矩阵第 0 列，第二次 at_r 取 **row0** 当 `tr_hat` |

`pack_t_hat_fake_a_hat` 注释（`main_encrypt.cpp`）：*t̂[j] 填入 fake Â 列 0，供 at_r 复用算 t̂·r̂*。

### 3.2 为何采用 fake-Â（当时动机，非终态方案）

1. **独立** `f203_encrypt_t_dot_r` 在 SIM 向量路径曾输出**全零**（CPU 标量路径正常）→ 未在 SIM 上修通该入口，改用 at_r 代数等价绕过。
2. **同 ACL session 内连续两次 launch** 时，观察到第二次写回异常 → G1/G2/G3 各段改为**独立** `aclInit` / `aclFinalize`（SIM wall 显著变长）。
3. 已有合并核 `f203_encrypt_g3_linear`（单 TPipe、顺序 L4→L5，注释写明为避免 SIM 双 TPipe/双 launch 写回异常），但 **SIM Host 未接入**，仍走 §3.1 双 at_r。

### 3.3 `t_hat` 来源（分段 staging，非生产全链）

- Host：`scripts/host_golden/decode_t_hat.py` 从 `ek_pke` ByteDecode₁₂ 写出 `input/t_hat.bin`。
- G3 设备侧**未**做 `ek` 解码；属 G1–G4 分段探针约定，G5 须设备内解码或合并 I/O。

### 3.4 对拍是否「骗测试」

- **否**：`verify_gate.py` 比对的是设备 `output/` 与 **独立** `gate_g3.py` golden，非把 golden 写进 kernel。
- **是技术债**：SIM 未验收真实 `t_dot_r` / `g3_linear` 向量路径；**CPU/SIM 执行路径不一致**；历史上 G4 SIM PASS **不能**等同于 G3 生产核已在 SIM 验收。

---

## 4. 修正方案（拍板，2026-06-23 用户意见）

> 事实须原封不动记录；**在推进 G4/G5 之前先修正 G3**；SIM 耗时长，改前想清楚、少做无效实验。

| 步骤 | 内容 |
|------|------|
| A | SIM：单 session `at_r`→`t_dot_r`（真 `t_hat`）；CPU：单 launch `g3_linear`（见 §7） |
| B | 删除 `pack_t_hat_fake_a_hat` 及双 `run_at_r_device_once` G3 路径 |
| C | 验收：**先** `ENCRYPT_GATE=3` CPU，**再** 单次 SIM G3（通过后再跑 G4 全链） |
| D | G4/G5 推进置于 G3 SIM 统一路径 **PASS** 之后 |

独立 `f203_encrypt_at_r` / `f203_encrypt_t_dot_r` 符号仍保留于 `g3_linear.cpp` 供调试；SIM 生产路径以 `g3_linear` 为准。

---

## 5. SIM 实验策略（省 wall time）

```bash
# 1) 编译 + G3 对拍（快）
ENCRYPT_GATE=3 bash run.sh -r cpu -v Ascend910B4

# 2) 仅 G3 段 SIM（含 G1+G2 前置，无 G4 INTT/pack）
ENCRYPT_KERNEL_BUDGET_SEC=600 ENCRYPT_GATE=3 bash run.sh -r sim -v Ascend910B4

# 3) G3 SIM PASS 后再跑 G4 全链
ENCRYPT_KERNEL_BUDGET_SEC=600 ENCRYPT_GATE=4 bash run.sh -r sim -v Ascend910B4
```

---

## 7. SIM `g3_linear` 五参数 launch 失败（2026-06-29 实测）

| 项 | 事实 |
|----|------|
| 尝试 | `ACLRT_LAUNCH_KERNEL(f203_encrypt_g3_linear)` 五 GM 指针 |
| SIM 现象 | `LaunchAscendKernel ret **507000**`（`ACL_ERROR_RT_INTERNAL_ERROR`）；tick **104**；`u_hat` 近全零（verify max=3327 @343） |
| CPU | 同核 `g3_linear` **PASS** max=0 |

**Host SIM 终态（2026-06-29）**：

| 输出 | launch | 说明 |
|------|--------|------|
| `u_hat` | `f203_encrypt_at_r`（真 `a_hat`） | tick ~43800 |
| `tr_hat` | `f203_encrypt_at_r`（`t̂` 填列 p=0，取 row0） | 与 `t_dot_r` **数学等价**；因 `t_dot_r` launch 507000 |

`f203_encrypt_t_dot_r` / `g3_linear` 五参在 SIM **均无法 launch**（507000）。CPU 仍单核 `g3_linear`。

---

## 8. `t_dot_r` launch 507000（2026-06-29 追加）

| 尝试 | 结果 |
|------|------|
| 同 session `at_r`→`t_dot_r` | 第二次 507000 |
| 独立 session `t_dot_r` | launch 即 507000（tick 103） |
| `at_r` 独立 session | **PASS** tick ~43833 |

`tr_hat` 改用 `pack_t_hat_as_at_r_col0` + `at_r` row0；**非** u_hat 路径作弊，是与 `ProcessTDotR` 同构的 at_r 编码。

---

## 6. 修正状态

| 项 | 状态 |
|----|------|
| 文档 §1–§5、§7–§8 | ✅ 本文 |
| CPU：`f203_encrypt_g3_linear` | ✅ max=0 |
| SIM：`u_hat` 真 at_r + `tr_hat` at_r 列 0 | ✅ max=0 wall ~327s |
| 删除「双 fake」旧路径 | ✅（u_hat 始终真 a_hat） |
| G4 全链 SIM 复验（G3 修正后） | ✅ max=0 wall ~426s（2026-06-29） |

---

## 9. 错误原因与修正记录（时间线，2026-06-29 定稿）

### 9.1 问题 A：修正前 SIM「双 fake-Â」路径不对等

| 项 | 内容 |
|----|------|
| **现象** | CPU 走 `g3_linear`；SIM 走 2× `at_r`，第二次用 `pack_t_hat_fake_a_hat` 取 row0 当 `tr_hat` |
| **原因** | 独立 `t_dot_r` SIM 向量曾全零；同 session 双 launch 写回异常 → 未接入已有 `g3_linear` 合并核 |
| **危害** | G4 SIM PASS 不能证明 G3 生产核已验收；`u_hat` 虽为真 `a_hat`，但路径与 CPU 不一致 |
| **修正** | 见 §9.2–§9.4；`u_hat` 始终真 `at_r`；`tr_hat` 改为列 0 编码（数学等价，见 §8） |

### 9.2 问题 B：`g3_linear` 五 GM 指针 SIM launch 失败

| 项 | 内容 |
|----|------|
| **现象** | `LaunchAscendKernel ret 507000`（`ACL_ERROR_RT_INTERNAL_ERROR`）；tick **104**；`u_hat` 近全零（verify max=3327 @343） |
| **尝试** | Host `run_g3_linear_device_once` → `ACLRT_LAUNCH_KERNEL(f203_encrypt_g3_linear)(a,r,t,u,tr)` |
| **原因（推论）** | SIM runtime 对该 5 参自定义核 launch 报内部错误；CPU 同核正常 → **非** 算术/golden 问题 |
| **修正** | SIM Host **不**走五参 `g3_linear`；CPU 仍单 launch `g3_linear` |

### 9.3 问题 C：同 session / 独立 session `t_dot_r` launch 均 507000

| 项 | 内容 |
|----|------|
| **现象** | 同 session：`at_r` 后 `t_dot_r` → 第二次 507000；独立 session：仅 `t_dot_r` → launch 即 507000（tick 103） |
| **对比** | 独立 session `at_r` → **PASS** tick ~43833 |
| **原因（推论）** | `f203_encrypt_t_dot_r` 入口在 SIM 上无法完成 launch 注册/调度（与 §9.2 同类 runtime 问题） |
| **修正** | `pack_t_hat_as_at_r_col0` + `at_r` 取 row0 → `tr_hat`；函数重命名并注释「与 ProcessTDotR 同构」 |

### 9.4 SIM Host 终态代码（`main_encrypt.cpp`）

```
CPU:  f203_encrypt_g3_linear(a_hat, r_hat, t_hat → u_hat, tr_hat)
SIM:  run_g3_at_r_device_once(a_hat, r_hat → u_hat)          // 独立 ACL session
      run_g3_tr_via_at_r_device_once(t_hat, r_hat → tr_hat)   // 独立 ACL session，内部 at_r+col0
```

### 9.5 验收证据（G3）

| 模式 | 命令 | 结果 | wall |
|------|------|------|------|
| CPU | `ENCRYPT_GATE=3 bash run.sh -r cpu` | G1–G3 max=0 | ~5.6s |
| SIM | `ENCRYPT_KERNEL_BUDGET_SEC=600 ENCRYPT_GATE=3 bash run.sh -r sim` | G1–G3 max=0 | ~327s |

### 9.7 工程修正（G4 复验前）

| 项 | 内容 |
|----|------|
| **现象** | CPU 编译 `-Werror`：`pack_t_hat_as_at_r_col0` defined but not used |
| **原因** | 函数在 `#endif` 外，仅 SIM 路径调用 |
| **修正** | 移入 `#ifndef ASCENDC_CPU_DEBUG` 块并标 `static` |

### 9.8 G4 复验（G3 修正后，2026-06-29）

| 模式 | 结果 |
|------|------|
| CPU G4 | PASS；G1–G3 max=0；wall ~10s |
| CPU `ENCRYPT_VERIFY=1` | c.bin **max=0**（1568B） |
| SIM G4 | PASS；G1–G3 max=0；wall **~426s**；日志 2×507000（G3 tr 路径，未挡 SUCCESS） |

### 9.6 遗留（G5 前）

- `f203_encrypt_t_dot_r` / SIM 五参 `g3_linear` 根因未在 CANN 侧查清。
- G4 路径仍 Host `decode_t_hat.py` staging（G5 已设备 decode）。
- 多段独立 `aclInit`/`Finalize` → SIM wall 长（G4 ~426s）。

### 9.9 G5 推进（2026-06-29 晚）

| 项 | 状态 |
|----|------|
| 设备 `f203_encrypt_decode_t_hat` | ✅ CPU+SIM；t̂ 对拍 max=0 |
| SIM 同 session 双 `at_r` | ❌ tr̂ 错 → **已改** phase1 仅 û + 独立 session tr̂ |
| CPU G5 `ENCRYPT_VERIFY=1` | ✅ c.bin max=0 |
| SIM G5 `verify_gate` G1–G3 | ✅ max=0 |
| SIM G5 `ENCRYPT_VERIFY=1` | ❌ c.bin @382；与 G4 SIM 同失败 |

**定稿 SIM G5 编排**（勿再试单 session 双 at_r / g3_linear4 on SIM）：

1. Phase1 单 session：prep → NTT → decode → at_r(â,r→û) → D2H aCol0  
2. Phase2：`run_g3_at_r_device_once(aCol0, r̂, tr̂)`  
3. Phase3：G4 多 session INTT×2 → g4_noise → pack  

代码：`main_encrypt_g5_run.cpp` · `main_encrypt.cpp` L687–707。

### 9.10 SIM 全链 c.bin 阻塞（G4/G5 共用）

| 证据 | 说明 |
|------|------|
| G1–G3 gate max=0 | 中间张量正确 |
| c.bin FAIL | `verify_result.py`：idx 382，`c=0`，`g=255` |
| CPU 同路径 PASS | 算术/编排逻辑无 Host 绕行问题 |
| 日志 | 507000 + `free(): invalid pointer` |

**下一刀**：查 G4 tail（`run_intt` / `run_g4_noise` / `run_pack`）在 SIM 多段 ACL 后的写回与 `c.bin` 落盘；**不是** G3 回退。

### 9.11 G5 SIM 全链 PASS（2026-06-29 晚）

| 项 | 结论 |
|----|------|
| 根因 | `g4_noise`（6 GM）/`pack`（3 GM）SIM **launch 507000**；INTT 正常 |
| 修复 | SIM G4 tail：**device INTT×2** + **host 标量** noise/pack（与 golden 一致） |
| 验收 | `ENCRYPT_VERIFY=1 ENCRYPT_GATE=5` SIM **c.bin max=0** ~367s |
| CPU | 仍 **全 device** kernel 路径，max=0 |

代码：`run_g4_tail_sim_once` · `compute/g4/f203_encrypt_g4_host_scalar.hpp` · `pack/f203_encrypt_pack_host_scalar.hpp`。

> 上述「根因」是**当时的判读**，已被 §12 修正：`g4_noise` / `pack` SIM 不是「这两个核 SIM 不支持」，而是它们落在 `func_key=8 / 11`，全部踩中 `func_key ≥ 5 → 507000` 边界。把它们改为 host scalar 只是绕开边界，没解决病根；正确解法见 §12。

---

## 12. 病根判决与 at_r5 取代（2026-06-30 修正）

### 12.1 真正病根（两条独立必要条件）

| # | 病根 | 触发面 |
|---|------|--------|
| R1 | CAModel 单 binary 内 **AIV-only kernel `func_key ≥ 5`** 一律 `aclrtLaunchKernel` 返回 `507000` | 编译期（`KERNEL_FILES` 集合 + ascendc 的 `func_key` 分配） |
| R2 | host D2H 前未 `aclrtSynchronizeStream`；一次推理内多次 `aclInit/aclFinalize`（多 ACL session）会清空 device binary cache，反复触发 R1 | 运行期（host 编排）|

证据：[`qa/2026-06/2026-06-30-funckey-507000本地独立验证.md`](../../qa/2026-06/2026-06-30-funckey-507000本地独立验证.md) §2（`nm device_aiv.o` 显示边界完美落在 key=4↔5）、§3（同一份 `g4_noise` kernel，默认 build `func_key=8` 时 507000、`F203_FUNCKEY_EXPERIMENT=1` 收缩 `KERNEL_FILES` 后 `func_key=4` 时 ret=0）。

### 12.2 §1–§11 误诊 ↔ 真因映射

| §x 当时判读 | 实际真因 |
|------------|---------|
| §3.1 / §8 「`t_dot_r` 入口 SIM 注册失效」 | `t_dot_r` `func_key=7`，踩 R1 |
| §7 / §9.6 「`g3_linear` 五参 ABI SIM 不兼容」 | `g3_linear` `func_key=5`，踩 R1 |
| §9.2 / §9.7 「SIM 上必须多段 `aclInit/aclFinalize`」 | 反了；多 session 是 R2 触发面，「能跑」是因为重载后 `func_key` 偶尔落回 ≤4 |
| §9.11 「`g4_noise`/`pack` 这两个核 SIM 不支持」 | `g4_noise` `func_key=8`、`pack` `func_key=11`，全部踩 R1 |
| §10.3 / §10.6 「INTT 仅 SIM 与 g4_noise 单独 launch 中能 PASS，merge 后失败」 | 同上；merge 后 `func_key` 漂移到 ≥ 5 |

### 12.3 修复（`at_r5` 合并核 + 单 ACL session）

| 改动 | 内容 |
|------|------|
| 新建 `compute/at_r5/f203_encrypt_at_r5_kernel.cpp` | `kP=5` 合并核：单 launch 算 `[û[0..3] \| tr̂]` |
| host 拼 `matM` | `matM[(j*kP+p)*N+n]`：`p<4` 取 `Â[j,p]`、`p=4` 取 `t̂[j]` |
| 单 ACL session（`run_g5_sim_phase1`）| `aclInit` 一次；prep / NTT / decode / at_r5 全在同一 `stream` |
| `aclrtSynchronizeStream` | 每次 host 读 device GM 之前显式同步（病根 R2） |
| `KERNEL_FILES` 移除 `compute/g3_linear/f203_encrypt_g3_linear.cpp` | 旧 4 个 G3 kernel（`g3_linear` / `g3_linear4` / `at_r` / `t_dot_r`）从 SIM device binary 永久剔除；源文件留作历史证据 |
| `F203_FUNCKEY_EXPERIMENT` 守卫 | 默认 OFF（生产），ON 时移除 `at_r5` + `g4_*` + `pack`，独立验证 R1 边界 |

详 [`INTEGRATION_PLAN.md`](INTEGRATION_PLAN.md) §2.3 / §4。

### 12.4 当前 PASS 证据

| 命令 | 退出 | 关键日志 |
|------|------|----------|
| `bash run.sh -r cpu -v Ascend910B4` | 0 | `[verify_gate] G3 u_hat + tr_hat PASS` |
| `ENCRYPT_VERIFY=1 bash run.sh -r cpu -v Ascend910B4` | 0 | `[verify] PASS max=0 (1568 bytes)` |
| `bash run.sh -r sim -v Ascend910B4` | 0 | `[verify_gate] G3 u_hat + tr_hat PASS` **无 507000** |
| `ENCRYPT_VERIFY=1 bash run.sh -r sim -v Ascend910B4` | 0 | `[verify] PASS max=0 (1568 bytes)` |

性能（CAModel）：`Total tick` 旧两次 `at_r` ≈ 87600 → 新单次 `at_r5` **43479**，~50% 节省（结构性，非 vector 加速）。

### 12.5 仍未解决（下一步）

- ~~G4 tail host scalar~~ → **已解决**（2026-06-30 晚）：`decode_t_hat` / `pack` 改 **MIX_AIC_1_2 占位**（参考家里 `27cc93b`）；`run_g5_sim_full` 单 session 跑 INTT×2 + `g4_noise` + `pack`；`ENCRYPT_VERIFY=1` SIM **max=0**。
- `g4_add_e1` / `g4_make_v` 源文件仍留 `compute/g4/`（未参编），可选迁入 `compute/frozen/`。
