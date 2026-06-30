# 2026-06-30 Encrypt 单 session 重建 · SIM func_key≥5 → 507000 病根

关键词：Encrypt Alg.14、单 ACL session、新探针 `fix-f203-alg14-encrypt-2launch-k4`、G3 合并核 `at_r5`、SIM 507000、AIV binary func_key、CAModel、受控实验。

## 1. 背景

按用户要求**新建目录** `ascendc-tests/fix-f203-alg14-encrypt-2launch-k4/`，参考 KeyGen 单 session 模板重搭 Encrypt 全链（不再多 session）。CPU 全链 ✅（G1/G2/G3 verify_gate max=0、c.bin max=0）。SIM 卡在 G3。

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

## 5. 现状

- CPU 全链 PASS（可汇报）。
- 代码当前为设计态（`kP5=5`、main 走 at_r5）；DIAG 受控实验保留为 `-DDIAG_G3_PROVEN_AT_R` 宏开关。
