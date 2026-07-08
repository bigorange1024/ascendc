# 2026-07-08 — Compress/ByteEncode 扩档、tail pack、四算子宏分层定稿、tiling 规范化、sepolyvec8 修复与 CAModel FPE 根因

## 1. Compress / Decompress 探针

| 旧目录 | 新目录 |
|--------|--------|
| `pass-f203-compress-d4-d10-vec-k4` | **`pass-f203-compress-d-vec-k4`** |
| `pass-f203-decompress-d4-d10-vec-k4` | **`pass-f203-decompress-d-vec-k4`** |

- 验收 **d∈{4,5,10,11}** CPU+SIM PASS
- d=5：int32 Barrett（bias **`1<<26`**）；d=11：cast_div 商向量
- 指南：`docs/notes/F203-Compress-Decompress-向量实现指南.md`

SIM tick（compress，910B4）：d=4 **3247** · d=5 **3121** · d=10 **3449** · d=11 **3399**

## 2. ByteEncode / ByteDecode 探针

| 旧目录 | 新目录 |
|--------|--------|
| `pass-f203-byteencode-d4-d10-vec-k4` | **`pass-f203-byteencode-d-vec-k4`** |
| `pass-f203-alg6-bytedecode-d4-d10-vec-k4` | **`pass-f203-alg6-bytedecode-d-vec-k4`** |

- 扩展 **d=5/11**：8 系数/组 + 向量 `mask_low_bits` + 分组 pack/unpack（与 ml-kem-native 比特布局 / Alg.5 0-diff）
- 全档 **d∈{4,5,10,11}** CPU+SIM PASS

SIM tick（910B4）：encode d=4 **5435** · d=5 **5537** · d=10 **6455** · d=11 **6568**；decode d=4 **9186** · d=5 **5696** · d=10 **6546** · d=11 **6641**

### 2b. ByteEncode d=5/d=11 真·向量 pack 实验（`BYTE_ENCODE_D_VEC=2`，更慢，不采纳）

**缘起**：审 `pass-fix-f203-alg14-lines2-24-encrypt-compute-tail-k4` 时问「Compress/ByteEncode 是否该抄向量写法」。
核对：Compress 已抄向量（`compress-d-vec`）；**ByteEncode 的 bit-pack 本就标量逐组**——抄源 `byteencode-d-vec` 的 pack 两分支都是标量，"vec" 仅指低位掩码前缀。真·向量 pack 只在 **d=12**（`byteencode12-vec-k4`）存在，因 **2×12=24bit=3B 字节对齐**。

**实验**：在 `pass-f203-byteencode-d-vec-k4` 加 `BYTE_ENCODE_D_VEC=2`，仿 d=12 方案对 d=5/d=11 做真·向量 pack
（`Gather` 取每组 8 个 position-lane → 向量算 byte-lane → 每 4 组拼整字 → 批量 `DataCopy`；拼字仍标量 `GetValue`）。

**结论：正确但更慢，不采纳**（tail 保持标量逐组 pack）。

| d | 正确性 CPU/SIM | VEC=1 标量 SIM tick | VEC=2 真向量 SIM tick |
|---|---|---|---|
| 5 | 0-diff / 0-diff | **5464** | 5839（+7%） |
| 11 | 0-diff / 0-diff | **6604** | 7404（+12%） |

**原因**：d=5/d=11 每系数 5/11bit 不按整字节对齐，拼字无法像 d=12 靠 3B 整字省，仍需逐 lane 标量 GetValue；
真·向量净增 8×`Gather`+向量算术，被 Gather 开销吃掉（去掩码/合并 barrier 后差距缩小仍慢）。
详见探针 `STATUS.md` §VEC=2；API 索引已追加 Gather/CreateVecIndex 查阅记录。

### 2c. 四算子宏分层定稿（Compress / Decompress / Encode / Decode）

**决策**：代码**保留**向量实验路径，宏切换；**默认只激活已验收且 SIM 不劣于标量的路径**。定稿笔记：[`docs/notes/F203-ByteEncode-ByteDecode-d-向量与标量选型.md`](../../docs/notes/F203-ByteEncode-ByteDecode-d-向量与标量选型.md)。

| 算子 | 探针 | 宏 | 默认 | 默认激活向量？ | 说明 |
|------|------|-----|------|----------------|------|
| Compress_d | `pass-f203-compress-d-vec-k4` | `COMPRESS_D_VEC` | **1** | **是** | per-lane Barrett / cast_div；tail 抄此 |
| Decompress_d | `pass-f203-decompress-d-vec-k4` | `DECOMPRESS_D_VEC` | **1** | **是** | `Muls+Adds+ShiftRight` 全档；Decrypt 链 |
| ByteEncode_d | `pass-f203-byteencode-d-vec-k4` | `BYTE_ENCODE_D_VEC` | **1** | 部分 | d=5/11：**标量逐组 pack**；`VEC=2` Gather pack **保留不激活**（+7%/+12% tick） |
| ByteDecode_d | `pass-f203-alg6-bytedecode-d-vec-k4` | `BYTE_DECODE_D_VEC` | **1** | d=4 仅 | d=5/11：`VEC=0` 与 `VEC=1` **同体**标量 unpack；**无 VEC=2** |

**Encrypt tail**（`pass-fix-f203-alg14-lines2-24-encrypt-compute-tail-k4`）：Launch 2 = **Compress 向量 + ByteEncode 标量 pack**；`STATUS.md` / `INTEGRATION_PLAN.md` §2.3 已备注。

**Decrypt 链**：`ByteDecode`（d=5/11 标量）→ `Decompress`（向量）。

**同步**：四探针 `*_config.hpp` / `*_vec.hpp` 文件头注释、`STATUS.md`、compress 指南 §5、`docs/notes/INDEX.md`、`ascendc-tests/INDEX.md` 已链到定稿笔记。

## 3. Alg.14 tail pack 探针

目录：`pass-fix-f203-alg14-lines20-22-23-24-encrypt-pack-k4`

| 行 | 内容 | 状态 |
|----|------|------|
| 20 | m → μ_embed（输出，不加 v） | ✓ |
| 22–23 | Compress₁₁(u) + ByteEncode → c₁ | ✓ |
| 24 | Compress₅(v) + ByteEncode → c₂ | ✓ |

- Compress 向量抄自 `pass-f203-compress-d-vec-k4`
- ByteEncode：**分组 pack** 抄自 `pass-f203-byteencode-d-vec-k4`（替代 Alg.5 比特流标量）
- 功能验证探针，不晋级；合并 compute 时 **抄码**、禁止跨探针 `#include`

| 模式 | 结果 |
|------|------|
| CPU | PASS（mu_embed、c max=0） |
| SIM | PASS **56259** tick（原比特流 ~227k–256k，约 **4×** 下降） |

## 4. 内核超时口径修正（全仓）

**决策**：`KERNEL_COMPUTE_BUDGET_SEC` = 各用例 `run.sh` **防挂死**预算（60s–1800s）；**~15s** 仅为 **ML-KEM Tag5T NTT 全流程** SIM **性能定标**，不适用于 KeyGen / Encrypt 全链 / Compress / tail pack 等。

| 产出 | 路径 |
|------|------|
| 定稿 | `docs/engineering/内核计算超时与性能定标.md` |
| Rule | `.cursor/rules/ascendc-development.mdc`「用例计算超时」 |
| 复现指南 | `docs/engineering/环境复现与开发指南.md` §6.5、§12–§14 |
| Skill | `ascendc-engineering-notes` §8 |

## 5. 遗留 / 下一步

| 项 | 说明 |
|----|------|
| **T17** | **PASS** compute+tail — [`pass-fix-f203-alg14-lines2-24-encrypt-compute-tail-k4`](../ascendc-tests/pass-fix-f203-alg14-lines2-24-encrypt-compute-tail-k4/) | 已关闭 → 见已关闭表 |
| **T17-next** | 全链 Encrypt — [`pass-fix-f203-alg14-pke-encrypt-device-k4`](../ascendc-tests/pass-fix-f203-alg14-pke-encrypt-device-k4/) · **2 launch SIM** | 方案定稿 |

## 6. 证据

```bash
# byteencode d=5
F203_BYTE_ENCODE_D=5 SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4  # PASS totalTick=5537

# tail pack
cd ascendc-tests/pass-fix-f203-alg14-lines20-22-23-24-encrypt-pack-k4
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4  # PASS totalTick=56259
```

## 7. tiling 文件规范化（全量审计 + 收敛）

**规范判据**（对齐 AscendC 样例 `matmul_custom_tiling.cpp` / `sepolyvec8_ntt_custom_tiling.cpp` 与已改的 tail 探针）：
运行时 `TilingData` 由**宿主 C++**（`GenerateTiling` / `FillVecTiling`）生成，**不**由 Python 落 `input/tiling.bin` 再 `ReadFile`；device 编译期几何仍走 `namespace tiling` constexpr。

全量审计 34 个活跃用例：多数已规范（全链探针 host 现场构造 TilingData、alg11 系 constexpr 直取、14 个无 tiling 结构）。
「Python 落 `tiling.bin` + 运行时读」不规范候选 6 个；**本轮只改固定值的 2 个**（用户定 `fixed_only`，带运行时 `mixPass` 开关的 vec-k4-v2 / stage123 / toy-mix 暂不动）：

| 用例 | 新增 | 值 | CPU / SIM |
|------|------|----|-----------|
| `pass-fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4` | `f203_encrypt_tiling.cpp` | `n,k,3` | max=0 / tick 122134 ✓ |
| `pass-fix-f203-2s1e-byteencode12-vec-k4` | `byte_encode12_tiling.cpp` | `n` | PASS / PASS ✓ |

均：main 调 `GenerateTiling` 替代 `tiling.bin` 读、CMake 加源、`gen_data.py` 删 `struct.pack`+`import struct`。

## 8. exp-sepolyvec8-ntt-k8 修复 + CAModel SIM FPE 根因

**修复**：该探针 `CMakeLists.txt`、`scripts/{gen_data,verify_result,verify_stage}.py` 在某次同步中丢失、完全跑不起来。
确认 `main.cpp/tiling.h/run.sh/kernel` 与 backup `v0.1_20260626191947` 逐字节一致后，从该快照**还原**缺失文件（找回误删，非照 customspec 新写）。CPU 立即通过（md5 一致、max_abs_diff=0）。

**SIM FPE 根因**（backtrace 抓取）：

```
SIGFPE = _Mod_range_hashing::operator()          # unordered_map 桶数=0 → hash % 0
 ← unordered_map<uint,uint>::find
 ← cce::runtime::Config::InitHardwareInfo950()    # CAModel 硬件表初始化 CANN bug
 ← Runtime 启动 ← rtSetKernelDfxInfoCallback
 ← Adx::KernelDfxDumper::EnableDfxDumper()
 ← Adx::DumpManager::DumpManager()  [_dl_init]     # libascend_dump.so 静态初始化
```

- **触发开关 = `ASCEND_WORK_PATH` 被设置**（逐变量二分：`CAMODEL_LOG_PATH`/`ASCEND_PROCESS_LOG_PATH` 单独设不崩）。设了它，ADX DFX dumper 在 **main 之前的静态初始化**就 boot CAModel runtime，命中 `InitHardwareInfo950()` 对空 map 取模除零。
- **与 kernel 无关**：不设该 env 时 SIM 对拍 `max_abs_diff=0`、md5 与 golden 一致。
- **本探针二进制特有**：#1 二进制同 env 单设 `ASCEND_WORK_PATH` 不崩；两二进制 `DT_NEEDED` 顺序与依赖集合完全相同 → 差异在该二进制静态初始化时序稳定命中此 CANN bug。

**修复（保留）**：
- `scripts/camodel_sim_log.sh`：加默认不变的 opt-out，`CAMODEL_SKIP_ADX_WORK_PATH=1` 时 `unset ASCEND_WORK_PATH`。
- 本探针 `run.sh` SIM 分支 `export CAMODEL_SKIP_ADX_WORK_PATH=1` 规避 FPE，并补 `camodel_sim_collect_stray` 把 CAModel dump 收进 `sim_log`（不设 WORK_PATH 时 dump 落 cwd）。
- 验收：`SIM_DIRECT=1 bash run.sh -r sim` → `max_abs_diff=0` `[SUCCESS]`，根目录 0 stray；#2 复跑 SIM 确认共享脚本改动无回归。

## 9. Phase C：SIM 单 launch 内联 tail pack（2026-07-08）

**决策**：SIM 从 2 launch（compute + 独立 pack）改为 **1 launch**；tail pack 内联至 `f203_encrypt_l18_l19` AIV 尾部，双 AIV 分片写 c（sub0: c₁[0..1]+c₂；sub1: c₁[2..3]）。CPU 仍 3 launch compute + 1 pack。

**实现**：`compute/f203_tail_pack_ops.hpp`（`pack_one_u_poly_d11` / `pack_v_poly_d5` / `tail_pack_shard_gm`）；`f203_encrypt_l18_l19` 增参 `cGm`；`main.cpp` SIM 只调一次融合核。

**验收**（910B4，改前备份 `backup/v0.1_20260708165835`）：

| 模式 | c/u/v | tick |
|------|-------|------|
| CPU | max=0 | ~27s |
| SIM | max=0 | **154781**（原 2 launch **172934**，约 −10%） |

根目录无 stray dump。

## 10. 晋级 pass- 前缀 + 文档定稿（2026-07-08）

**决策**：`fix-f203-alg14-lines2-24-encrypt-compute-tail-k4` → **`pass-fix-f203-alg14-lines2-24-encrypt-compute-tail-k4`**；作为 **Alg.14 compute+tail PASS 基线**，下一步在此上接 prep 做全链 Encrypt。

**文档**：`docs/notes/F203-Alg14-Encrypt-compute-tail-PASS技术总结.md`；`STATUS.md` / `INTEGRATION_PLAN.md` / `ascendc-tests/INDEX.md` 已刷新；T17 在 `qa/TODO.md` 标 **PASS**。

## 11. tail-pack 晋级 pass- + 全链方案（2026-07-08）

- `fix-f203-alg14-lines20-22-23-24-encrypt-pack-k4` → **`pass-fix-f203-alg14-lines20-22-23-24-encrypt-pack-k4`**
- 新建 [`pass-fix-f203-alg14-pke-encrypt-device-k4`](../ascendc-tests/pass-fix-f203-alg14-pke-encrypt-device-k4/)：prep + compute+tail **2 launch SIM** 集成方案（`INTEGRATION_PLAN.md`）

## 12. 全链设备 Encrypt 实现 + CPU/SIM PASS（2026-07-08）

**探针**：[`pass-fix-f203-alg14-pke-encrypt-device-k4`](../ascendc-tests/pass-fix-f203-alg14-pke-encrypt-device-k4/)（prep + compute+tail 集成，单 device session GM handoff）。

| 模式 | launch | 结果 |
|------|--------|------|
| CPU | prep + ntt_y/at_jp/intt_e1 + pack（5） | `c` max=0 |
| SIM | prep → l18_l19（含 e₂+=μ + 内联 pack）（2） | `c` max=0、`v` max=0；Total tick **626121**；根目录 0 stray dump |

**关键结论**：
- **a_hat/re handoff 零拷贝、无需转置**。三方索引一致：prep 存 `flat(p*K+j)=SampleNTT(ρ,j,p)` == correctness `build_a_hat` == compute `a_hat_offset_jp(j,p)=(j*K+p)` 读法；`re[0:4]=r(≡y)`、`[4:8]=e₁`、`[8]=e₂` 按字节偏移切片直喂 compute。
- **golden 复用**：`gen_data.py` 复制 correctness `input/{ek_pke,m,coins}` + `output/golden_c.bin`（`SEED_D=20260619`），本地派生 LUT 与 `golden_v`，并**三源一致性自检**（本地重算 c == correctness golden_c）。
- **CPU 的 v**：CPU 三 launch 无 k=8 INTT 不产 v，注入 `golden_v`（含 μ+e₂），不参与判定；c 仍为设备全链产出。
- SIM tick 626121 ≈ 方案估计（prep 470502 + compute 154781）。
- 已晋级 **`pass-fix-f203-alg14-pke-encrypt-device-k4`**；I/O 对齐 Alg.14（输入 ek+m+coins，**输出仅密文 c**，u/v 不落盘）。
