# Agent 交接 — 每日刷新（办公室 ↔ 家里）

> **用途**：公司与家里 Agent 的**唯一**短交接面；**每日**任务结束前覆盖刷新（不堆历史章节）。
> **详案**：`qa/YYYY-MM/` 当日纪要 · `docs/notes/` 定稿 · 各目录 `INDEX.md` / `STATUS.md`。
> **最后刷新**：2026-07-09（**Encrypt `#交付#` → `stable-mlkem-f203-pke-encrypt-k4`**；**验收权重：CPU 辅助 / SIM 主参考**；KAT×10+1 + roundtrip×10+1；T14a 关闭）

---

## ★ 强制写法（2026-07-08，新代码均须遵守）

| 类型 | 写法 | 禁止 |
|------|------|------|
| CPU vs SIM | `ASCENDC_BUILD_CPU` / `ASCENDC_BUILD_SIM` | per-probe 编译宏分叉 host |
| SIM host 拓扑 | `ASCENDC_SIM_HOST_MODE` + §3.3 登记表 | `F203_FEAS_*`、`KEM_DECAPS_SIM_2SESSION` 等 |
| 算法变体 | CMake 宏（CPU/SIM 同值） | env 切 launch |

详文：[`AscendC-CPU与SIM实现分叉开发指南.md`](docs/notes/AscendC-CPU与SIM实现分叉开发指南.md) §4.1 · [`library/shared/INDEX.md`](library/shared/INDEX.md)

**无 NPU 时验收权重**：Encrypt（及同类全链）— **SIM = 主参考**；**CPU = 辅助正确性**（可不同构 launch / `golden_v`）。见 [`F203-Alg14-Encrypt-交付口径-CPU辅助与SIM主参考.md`](docs/notes/F203-Alg14-Encrypt-交付口径-CPU辅助与SIM主参考.md)。

---

## ★ 当前真相（Encrypt + KEM，2026-07-09）

### Alg.14 Encrypt — **stable 交付** ★头条

算子：[`examples/stable/stable-mlkem-f203-pke-encrypt-k4`](examples/stable/stable-mlkem-f203-pke-encrypt-k4/)  
预研副本：[`examples/incubating/exp-mlkem-f203-pke-encrypt-k4`](examples/incubating/exp-mlkem-f203-pke-encrypt-k4/)（保留）  
规格：[`…-实现方案-customspec.pdf`](examples/stable/stable-mlkem-f203-pke-encrypt-k4/stable-mlkem-f203-pke-encrypt-k4-实现方案-customspec.pdf)

| 项 | 内容 |
|----|------|
| I/O | in `ek_pke`+`m`+`coins` → **out 仅 `c`**；Â/y/u/v **不落盘** |
| **SIM（主参考）** | `c` max=0；stable 复验 tick **627590**；0 stray dump |
| CPU（辅助） | `c` max=0；5 launch + `golden_v`（非与 SIM 同构） |
| 门禁 | liboqs KAT **CPU×10+SIM×1** · roundtrip **CPU×10+SIM×1** PASS |
| registry | [`fips203-mlkem1024-pke-encrypt-baseline-registry.md`](docs/specs/fips203-mlkem1024-pke-encrypt-baseline-registry.md) |

### Alg.14 完整 K-PKE.Encrypt（全链设备探针）— **完成（SIM 主 / CPU 辅）**

探针：[`pass-fix-f203-alg14-pke-encrypt-device-k4`](ascendc-tests/pass-fix-f203-alg14-pke-encrypt-device-k4/)（prep + compute+tail **GM handoff 零拷贝**串联）

| 项 | 内容 |
|----|------|
| I/O | **对齐 FIPS 203 Alg.14**：in `ek_pke`+`m`+`coins(r)` → **out 仅密文 `c`（`output/c.bin` 1568B）**；u/v 为设备内部中间量**不落盘** |
| 覆盖 | FIPS 行 1–22 全设备：行 1 `N←0`（无运算）、行 2 `t̂←ByteDecode₁₂(ek)`（`f203_encrypt_l18_l19` 核内 AIV0 解码，host 传 `tHat=nullptr`）、行 3–22（prep+compute+tail） |
| Launch | **SIM 2 launch**（生产主路径）；**CPU 5 launch**（辅助；v=`golden_v`） |
| 种子 | 全链唯一 `SEED_D=20260619`；输入/golden_c **复用** `fix-f203-alg14-pke-encrypt-correctness-k4` |
| 验收 | **SIM 主**：`c` max=0，Total tick **~626k**；CPU 辅：`c` max=0；默认 `SIM_DIRECT=1` + T7b |
| handoff | prep→compute a_hat/re **零拷贝无转置**；`re[9,256]` 切片 y=r(0)/e₁(4096B)/e₂(8192B) |
| 关闭 | 路线 11 LUT ROM（qa 2026-07-09） |

### Alg.14 Encrypt prep — **完成（CPU+SIM）**

探针：[`pass-fix-f203-alg14-lines3-15-encrypt-prep-k4`](ascendc-tests/pass-fix-f203-alg14-lines3-15-encrypt-prep-k4/)

| 项 | 内容 |
|----|------|
| 范围 | 行 3–7 `a_hat` + 行 8–15 `re`（r/e₁/e₂） |
| Launch | 单 launch `f203_encrypt_prep` |
| 验收 | `a_hat` / `re` max=0；CPU + SIM |
| SIM tick | ~470502 |

### Alg.14 Encrypt compute — **SIM 完成 / CPU 部分**

探针：[`pass-fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4`](ascendc-tests/pass-fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4/)

| 模式 | Launch | 对拍 | 判定 |
|------|--------|------|------|
| **SIM 默认** | 单 launch `f203_encrypt_l18_l19` | y_hat, u_ntt, u_tr, u, v | **完成**（行 2/18/19/21） |
| **CPU** | 3 launch（ntt_y→at_jp→intt_e1） | y_hat, u_ntt, u | **部分对照**（tikicpu 不得融合） |
| SIM 调试 | `ASCENDC_SIM_HOST_MODE=phased_launch` | 同 CPU | 分段调试 |

**定案**：û/uTr **驻留 UB** → INTT `ProcessFromLocal`；kP=5 pad→8；MIX GATE **4→8**；INTT flag **1/3**。

**已完成**：prep + compute + **tail** **GM 级拼接** → 见 §头条 `pass-fix-f203-alg14-pke-encrypt-device-k4`（SIM 2 launch）。compute+tail 单探针 `pass-fix-f203-alg14-lines2-24-encrypt-compute-tail-k4`。

### Alg.14 tail pack — **完成（CPU+SIM）**（2026-07-08）

探针：[`pass-fix-f203-alg14-lines20-22-23-24-encrypt-pack-k4`](ascendc-tests/pass-fix-f203-alg14-lines20-22-23-24-encrypt-pack-k4/)

| 项 | 内容 |
|----|------|
| 范围 | 行 20 μ_embed + 行 22–24 Compress/ByteEncode → `c` |
| ByteEncode | 分组 pack（抄 `pass-f203-byteencode-d-vec-k4`）；SIM **56259** tick（原比特流 ~227k） |
| 验收 | CPU+SIM；`mu_embed.bin` + `c.bin` max=0 |

### Compress / ByteEncode 单功能探针 — **d 全档 PASS**（2026-07-08）

| 探针 | 目录 | d |
|------|------|---|
| Compress | `pass-f203-compress-d-vec-k4` | 4/5/10/11 |
| Decompress | `pass-f203-decompress-d-vec-k4` | 4/5/10/11 |
| ByteEncode | `pass-f203-byteencode-d-vec-k4` | 4/5/10/11 |
| ByteDecode | `pass-f203-alg6-bytedecode-d-vec-k4` | 4/5/10/11 |

指南：`docs/notes/F203-Compress-Decompress-向量实现指南.md`

### 内核超时口径（2026-07-08）

- `KERNEL_COMPUTE_BUDGET_SEC`：**各用例 run.sh 防挂死**，非全仓 15s
- **~15s**：仅 ML-KEM **NTT 全流程** SIM 性能定标
- 定稿：[`docs/engineering/内核计算超时与性能定标.md`](docs/engineering/内核计算超时与性能定标.md)

### KEM Decaps — SIM 选项已统一（2026-07-08）

探针：[`fix-f203-alg21-kem-decaps-k4`](ascendc-tests/fix-f203-alg21-kem-decaps-k4/)

| SIM 默认 | `ASCENDC_SIM_HOST_MODE=decaps_2session`（或 unset） |
| 排障 | `decaps_1session` |
| Host | `ascendc::SimHostDecapsUse2Session()` |

KeyGen / Encaps / Decaps 分项 kat **CPU×10+SIM×1 PASS**（行为不变）。

---

## ★ 下一任务（P0）

1. **Alg.21 Decaps 单 session SIM 真修**（2-session 已是保底）。
2. **NPU 实机**：KEM + PKE 均未测（含 Encrypt stable）；上机前 SIM 仍为无实机时的主参考。
3. **T7b alg14 correctness `run.sh` 资源友好化**（`SKIP_REBUILD`/`CMAKE_BUILD_JOBS=2`，对齐 alg20；非功能项）。
4. **Decrypt `#交付#`**（T15a）：G4 探针已 PASS → exp/stable 路径待开。

---

## 验收命令（smoke）

```bash
# ★ Encrypt stable（主参考 = SIM；CPU 辅助）
cd examples/stable/stable-mlkem-f203-pke-encrypt-k4
bash run.sh -r sim -v Ascend910B4               # 主参考；c max=0，tick ~627k
bash run.sh -r cpu -v Ascend910B4               # 辅助
bash kat_liboqs_vs_ascendc.sh                   # CPU×10 + SIM×1
# 仓库根：bash scripts/roundtrip_pke_batch.sh

# 全链设备探针（对照）
cd ascendc-tests/pass-fix-f203-alg14-pke-encrypt-device-k4
bash run.sh -r sim -v Ascend910B4
bash run.sh -r cpu -v Ascend910B4

# Encrypt prep（双模式全 pass）
cd ../pass-fix-f203-alg14-lines3-15-encrypt-prep-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4

# Encrypt compute（SIM 全量；CPU 部分）
cd ../pass-fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4

# Encrypt tail pack
cd ../pass-fix-f203-alg14-lines20-22-23-24-encrypt-pack-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4

# KEM 分项 kat（回归）
bash scripts/liboqs_kem_keygen_batch.sh
bash scripts/kem_keypair_stash_bootstrap.sh
bash scripts/liboqs_kem_encaps_batch.sh
bash scripts/liboqs_kem_decaps_batch.sh
```

**WSL 约束**：`CMAKE_BUILD_JOBS=2`；compute 单 launch 默认 `KERNEL_COMPUTE_BUDGET_SEC=600`；勿并行多 SIM。默认 `bash run.sh -r sim` 已自动 `SIM_DIRECT=1`，**勿**再要求用户手动 export。

---

## 0. 对侧 Agent 必读（30 秒）

| 优先级 | 做什么 |
|--------|--------|
| 1 | `git pull` |
| 2 | 读本文件 **§当前真相 / §下一任务** |
| 3 | 当日纪要 [`qa/2026-07/2026-07-09-Encrypt默认SIM_DIRECT.md`](qa/2026-07/2026-07-09-Encrypt默认SIM_DIRECT.md) |
| 4 | Encrypt 交付口径 [`docs/notes/F203-Alg14-Encrypt-交付口径-CPU辅助与SIM主参考.md`](docs/notes/F203-Alg14-Encrypt-交付口径-CPU辅助与SIM主参考.md) · UB 驻留 [`docs/notes/F203-Encrypt-compute-行18-19-UB驻留技术总结.md`](docs/notes/F203-Encrypt-compute-行18-19-UB驻留技术总结.md) |
| 5 | [`qa/TODO.md`](qa/TODO.md) · [`.cursor/rules/ascendc-development.mdc`](.cursor/rules/ascendc-development.mdc) |
| 6 | **禁止**从 `frozen/` 抄码 · **中间态驻 UB**，GM 仅 dump 对拍 |

### 接手步骤（Encrypt stable / 探针）

1. 读 stable [`STATUS.md`](examples/stable/stable-mlkem-f203-pke-encrypt-k4/STATUS.md) + 探针 [`STATUS.md`](ascendc-tests/pass-fix-f203-alg14-pke-encrypt-device-k4/STATUS.md)（GM handoff：`f203_encrypt_full_layout.h`）。
2. 跑上方 ★ smoke：**先 SIM（主参考）**再 CPU（辅助）；确认 `output/c.bin` max=0、`output/` 仅 c、根目录 0 stray dump。
3. compute **不得**在 CPU 上试单融合 launch（tikicpu 死锁）；**SIM 为无 NPU 时的交付主参考**。golden 复用 correctness 输入，勿改日常 `SEED_D=20260619`。
4. 交付口径：[`docs/notes/F203-Alg14-Encrypt-交付口径-CPU辅助与SIM主参考.md`](docs/notes/F203-Alg14-Encrypt-交付口径-CPU辅助与SIM主参考.md)。

---

## 附录：关键路径速查

| 主题 | 路径 |
|------|------|
| **Encrypt stable 交付** | [`examples/stable/stable-mlkem-f203-pke-encrypt-k4/`](examples/stable/stable-mlkem-f203-pke-encrypt-k4/) · [交付口径](docs/notes/F203-Alg14-Encrypt-交付口径-CPU辅助与SIM主参考.md) · [registry](docs/specs/fips203-mlkem1024-pke-encrypt-baseline-registry.md) |
| **完整 Encrypt 探针** | [`INTEGRATION_PLAN.md`](ascendc-tests/pass-fix-f203-alg14-pke-encrypt-device-k4/INTEGRATION_PLAN.md) · [`STATUS.md`](ascendc-tests/pass-fix-f203-alg14-pke-encrypt-device-k4/STATUS.md) · handoff `f203_encrypt_full_layout.h` |
| prep 方案 | [`INTEGRATION_PLAN.md`](ascendc-tests/pass-fix-f203-alg14-lines3-15-encrypt-prep-k4/INTEGRATION_PLAN.md) |
| compute 方案 | [`INTEGRATION_PLAN.md`](ascendc-tests/pass-fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4/INTEGRATION_PLAN.md) |
| CPU/SIM 分叉 | [`docs/notes/AscendC-CPU与SIM实现分叉开发指南.md`](docs/notes/AscendC-CPU与SIM实现分叉开发指南.md) |
| UB 驻留原理 | [`docs/notes/F203-Encrypt-compute-行18-19-UB驻留技术总结.md`](docs/notes/F203-Encrypt-compute-行18-19-UB驻留技术总结.md) |
| 探针索引 | [`ascendc-tests/INDEX.md`](ascendc-tests/INDEX.md) |
| tail pack | [`INTEGRATION_PLAN.md`](ascendc-tests/pass-fix-f203-alg14-lines20-22-23-24-encrypt-pack-k4/INTEGRATION_PLAN.md) |
| Compress 指南 | [`docs/notes/F203-Compress-Decompress-向量实现指南.md`](docs/notes/F203-Compress-Decompress-向量实现指南.md) |
| 内核超时 | [`docs/engineering/内核计算超时与性能定标.md`](docs/engineering/内核计算超时与性能定标.md) |
