# 2026-06-30 Encrypt 单 session 重建 · SIM func_key≥5 → 507000 病根

> **⚠ 探针已冻结（2026-06-30 晚）**：`fix-f203-alg14-encrypt-2launch-k4` 已迁入 [`../../ascendc-tests/frozen/frozen-fix-f203-alg14-encrypt-2launch-k4/`](../../ascendc-tests/frozen/frozen-fix-f203-alg14-encrypt-2launch-k4/)。  
> 本文保留**家里 agent 原始讨论**；办公室未复验该树 PASS。活跃 Encrypt → [`stable-fips203-mlkem-pke-encrypt-k4`](../../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-pke-encrypt-k4/) G5。

关键词：Encrypt Alg.14、单 ACL session、新探针 `fix-f203-alg14-encrypt-2launch-k4`、G3 合并核 `at_r5`、SIM 507000、AIV binary func_key、CAModel、受控实验。

## 1. 背景

按用户要求**新建目录** `../../ascendc-tests/fix-f203-alg14-encrypt-2launch-k4/`，参考 KeyGen 单 session 模板重搭 Encrypt 全链（不再多 session）。CPU 全链 ✅（G1/G2/G3 verify_gate max=0、c.bin max=0）。SIM 卡在 G3。

## 2. 核心结论：CAModel 单 binary 内 AIV kernel **func_key ≥ 5 一律 507000；≤4 正常**

- 全链单 session 已打通（旧"多 session→507000"病根已除）；G1/G2 SIM max=0、device decode t̂ 正常。
- G3 合并核 `f203_encrypt_at_r5`（host 拼 5×4 矩阵 [Âᵀ|t̂]，一次出 [û|tr̂]）launch 即 **507000**，CAModel 同步执行 → core dump (signal 11)。

### 受控实验（逐步隔离，章法）

1. **proven `at_r`（4-poly û，`-DDIAG_G3_PROVEN_AT_R`）** → û PASS、**无 507000**。⇒ env/build/binary 正常；"MIX 后第 N 发 AIV 不可靠"假说**被推翻**（decode_t_hat、at_r 都在 MIX 之后成功）。
2. **`at_r5` 退化 `kP5=4`（逐字≈at_r）** → **仍 507000**。⇒ 不是 5-poly/8N scratch/5N DataCopy 增量。
3. **`nm device_aiv.o`**：`f203_encrypt_at_r5_5`（key5）符号存在、索引与 host_stub 一致。⇒ 不是没编进。
4. **host_stub func_key 对照**：AIV binary = marker(0) prep_a_hat(1) prep_re(2) g4_noise(3) **at_r(4)✅** **at_r5(5)❌** g3_linear(6)❌ g3_linear4(7)❌ t_dot_r(8)❌。历史 `G3_SIM_AUDIT` 中 g3_linear(6)/g3_linear4(7) 同样 507000。**边界恰在 4↔5。**

### 注意：func_key 不按定义序

文件里 g3_linear/g3_linear4 定义在前却拿 key6/7，at_r/at_r5 定义在后拿 key4/5。⇒ 不能靠重排定义可靠控制 func_key。

## 3. 修复方向（待验证）

把 **SIM 设备编译里 AIV kernel 总数压到 ≤5**，使 `at_r5` 落 key≤4：
- `at_r / g3_linear / g3_linear4 / t_dot_r`（SIM 不 launch）用 `#ifdef ASCENDC_CPU_DEBUG` **仅留 CPU**（CPU 独立 binary、func_key 无所谓；CPU 走 g3_linear4）；SIM 设备侧仅编 `at_r5`。
- `main_encrypt.cpp` 旧 staged 路径（line 29/324 引用 at_r/g3_linear/t_dot_r）对 SIM 关掉，避免 `aclrtlaunch_*.h` 缺失编译错。

**落地后第一件事**：`nm` 确认 at_r5 的 key≤4，再跑 SIM 看 507000 是否消失。若 key≤4 仍 507000，则病根另有其因，回受控实验。

## 4. 时间成本

SIM 崩溃 ~70s 出结果；**跑通全链 ~13 分钟**（G4/G5 tail 慢）。安排等待预算注意。

## 5. 解法落地 + 全链打通 ✅

### 病根 1 解法（func_key≥5）

SIM 设备侧 `g3_linear.cpp` **只编 `at_r5`**；`g3_linear/g3_linear4/at_r/t_dot_r` 用 `#ifdef ASCENDC_CPU_DEBUG`
仅留 CPU（CPU 独立 binary，func_key 无意义）→ SIM AIV 核 = marker/prep_a_hat/prep_re/g4_noise/at_r5 共 5 个。
`nm`/host_stub 复核：`at_r5` 落 **func_key 4**，507000 消失。删 `main_encrypt.cpp` 旧 staged gate<5 SIM G3
（独立 session at_r×N）+ 旧 `<<<>>>` `*_do` 壳。

### 病根 2（解 507000 后暴露）：û 全 0

at_r5 launch 成功但 `u_hat` 全 0。根因：g5_run 用 host 读回 `aHatDev/tHatDev` 拼 5×4 `matM`，**D2H 前未
`aclrtSynchronizeStream`** → prep_a_hat/decode_t_hat 异步未完成 → matM 的 Â 列取到 0 → û=Σ0·r̂=0。
proven `at_r` 直接在设备读 aHatDev 故无此问题（曾误导为「2nd AIV 不可靠」）。**解法**：打包 D2H 前加 sync。

**通用守则**：任何 host↔device 往返打包前必同步 stream。

### 现状（证据）

- **CPU 全链**：`[verify] PASS max=0 (1568 bytes)`、`[SUCCESS] (cpu)`。
- **SIM 全链**：G1/G2/G3 verify_gate max=0、`[verify] PASS max=0 (1568 bytes)`、`[SUCCESS] (sim)`；用例根无 stray dump。
- 代码设计态：`kP5=5`、SIM 走 at_r5（key4）；DIAG `-DDIAG_G3_PROVEN_AT_R` 已随 at_r CPU-only 移除（at_r 不再编入 SIM）。
