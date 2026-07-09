# 2026-06-25 — KeyGen prep 单 TPipe 与性能优化路线图

实现方案：[`ascendc-tests/pass-fix-f203-alg13-device-keygen-k4/INTEGRATION_PLAN.md`](../../ascendc-tests/pass-fix-f203-alg13-device-keygen-k4/INTEGRATION_PLAN.md) §1.1  
验收：[`STATUS.md`](../../ascendc-tests/pass-fix-f203-alg13-device-keygen-k4/STATUS.md)

---

## 背景

G4 全链 Step4：`f203_keygen_prep`（行 3–15）+ vec-k4-v2（行 16–20）= **2 设备 launch**。

| 里程碑 | prep tick（SIM） | 说明 |
|--------|------------------|------|
| Step3 分段（a_hat + presample，tick 相加） | ~809521 | `KEYGEN_PREP_LEGACY=1` |
| Step4 多 TPipe 融合 | 839178 (+3.7%) | 已回滚 |
| **Step4 单 TPipe（当前）** | **774357 (−4.3% vs 分段之和)** | `f203_keygen_prep_ub.hpp` |

全链设备 tick：**852359**（prep 774357 + compute 78002）。

**结论**：强行合并 launch **不保证**更快；单 TPipe + 一次 G + UB 复用才有净收益。分层探针 tick 须与集成路径对齐，否则「单用例验证」失去可比性。

---

## 实验原则（拍板）

1. **一轮一实验**：改一项 → CPU+SIM G4 → 记录 prep tick → 更新 STATUS §SIM 表。
2. **有效则叠加**：在上一项合入基线上继续下一编号。
3. **无效则回滚**：不保留负优化；STATUS 注明「已测否决」。
4. **编号 5 靠后**：2、1、3、4 全部测完后再议 Pipe 细同步。

---

## 实验顺序：2 → 1 → 3 → 4 →（5）

### 已合入（非待实验）

- **Step4 单 TPipe**：`BuildKeygenPrepSinglePipe`；`*WithUb` 子入口。
- **KeyGen 内一次 G**：`BuildRhoSigmaFromSeedD`（`Derand` + `SHA3-512` → ρ‖σ）。

### 2 — 子探针统一一次 G 派生 ρ‖σ（下一项）

| 项 | 内容 |
|----|------|
| 范围 | `pass-fix-f203-alg13-lines3-7-a-hat-k4`、`pass-fix-f203-alg13-lines8-15-se-k4` |
| 做法 | 抽出与 `f203_keygen_prep_ub.hpp` 同式的 `BuildRhoSigmaFromSeedD`；Â 用 ρ、presample 用 σ |
| 验收 | 各子探针 `run.sh` CPU+SIM PASS；G3/G2 与 KeyGen golden 仍一致 |
| 成功标准 | 子探针 tick 之和更接近 KeyGen prep 774357；**允许 G4 prep tick 几乎不变** |
| 风险 | 低（语义不变，仅少一次 Keccak） |

### 1 — PRF → CBD 跳过 `prf_out` GM

| 项 | 内容 |
|----|------|
| 范围 | `f203_keygen_prep_ub.hpp`、`RunShakePrfBatchUbWithUb` / `SamplePolyCbd2Batch8WithUb` |
| 做法 | PRF 的 `yUb` 不 `DataCopy` 到 `prf_out_gm`，直接喂 CBD；`prf_out.bin` 仅 `F203_PREP_DUMP_PRF` 等调试门控 |
| 验收 | G4 CPU+SIM；`src`/`a_hat` golden 不变 |
| 成功标准 | prep tick **可测下降**（粗估几个百分点以内） |
| 回滚 | 恢复 GM 中转 + 对拍路径 |

### 3 — Â 16 poly 流水（SHAKE ‖ rej 双缓冲）

| 项 | 内容 |
|----|------|
| 范围 | `f203_a_hat16_ub.hpp` `BuildAHat16ShardWithUb` 主循环 |
| 做法 | xof/d1/d2 双缓冲：poly *i* rej 时启动 poly *i+1* SHAKE128 |
| 验收 | G3 + G4；UB 峰值闭包；SIM 无 pem_lsu 类告警 |
| 成功标准 | prep tick **明显下降**（Â 占 prep ~90%） |
| 风险 | 高（UB、同步、对拍）；建议单独分支/门控 |

### 4 — 双 AIV Â（`F203_AHAT16_BLOCK_DIM=2`）

| 项 | 内容 |
|----|------|
| 范围 | `f203_keygen_prep` cmake + `f203_keygen_prep_ub.hpp`；presample 仍 block0 only |
| 做法 | `blockDim=2`；与 a_hat 探针 G5 同语义 |
| 验收 | G4 SIM；对比单 AIV prep tick |
| 参考 | a_hat 探针 **714150** vs 单 AIV ~734k（−2.7% tick）；墙钟曾持平 |
| 回滚 | `F203_AHAT16_BLOCK_DIM=1` |

### 5 — PipeBarrier 细粒度化（暂缓）

| 项 | 内容 |
|----|------|
| 范围 | Â 每 poly barrier；CBD 每行 barrier |
| 状态 | **2–4 测完后再议** |
| 参考 | [2026-06-22 CBD 讨论](2026-06-22-Alg8-CBD-eta2-性能优化讨论.md)：无 barrier SIM 虚低、对拍 FAIL |

---

## 不做（本阶段）

- `F203_AHAT16_BATCH_SHAKE=1`（+31% tick，已否决）
- prep+compute 单 launch（compute 仅 ~9% 设备 tick，ROI 低）
- 504B XOF 默认化（须独立门控实验）

---

## 遗留

- [x] **Opt-2**：子探针一次 G — **✅ 2026-06-25**（见下节）
- [x] **Opt-1**：PRF→CBD 无 GM — **❌ 已测否决 2026-06-25**（见下节）
- [x] **Opt-3**：Â 双缓冲 — **已关闭**（+3.3%，代码已回滚，见 qa §Opt-3）
- [x] **Opt-4**：prep 双 AIV Â — **✅ 已合入**（prep **454170**，−41.4%）
- [x] **Opt-5**：Pipe 细同步 — **✅ 部分合入**（Phase 1+5，2026-06-26）

---

## Opt-2 验收（2026-06-25）✅

**改动**：`F203Alg7::BuildRhoSigmaFromSeedD`（`f203_alg7_g.hpp`）；`BuildRhoFromSeedD` / presample `BuildSigmaFromSeedD` 委托；KeyGen prep 去重本地副本；presample cmake 增 `ALG7_INC`。

**验收**：a_hat / presample / G4 全链 CPU+SIM PASS。

| 探针 | SIM tick（Opt-2 后） | 对照（Opt-2 前） |
|------|----------------------|------------------|
| a_hat_16poly | **715121** | ~714210（KeyGen 分段基线） |
| presample V3 | **93800** | ~95311 |
| 分段之和 | **808921** | ~809521 |
| G4 prep 单核 | **774335** | 774357（≈噪声） |
| G4 全链设备 | **852305** | 852359 |

**结论**：语义不变；分层 tick 与集成 prep 更接近；**保留合入**，进入 **Opt-1**。

---

## Opt-1 验收（2026-06-25）❌ 已测否决

**改动**：`RunShakePrfBatchIntoBuf`（PRF yUb 驻 scratch，不写 GM）+ `SamplePolyCbd2Batch8FromPrfUb`（CBD 从 UB 批量读）；`F203_PREP_DUMP_PRF` 门控调试 dump。

**验收**：G4 CPU+SIM **PASS**（`src`/`a_hat`/全链 golden 一致）。

| 指标 | Opt-1 实测 | 基线（Opt-2 后） | Δ |
|------|------------|------------------|---|
| G4 prep tick | **822874** | **774335** | **+6.3%** |
| G4 compute tick | 77923 | ~78002 | ≈噪声 |
| G4 全链设备 | **900797** | **852305** | **+5.7%** |

**分析（粗）**：跳过 `prf_out` GM 省 1×1024B 写 + 8×128B 读，但 PRF 输出改驻 **Â rej scratch**（10KB 级 TBuf 前段），可能与 Â 阶段 UB 布局争用；CBD 改 **逐字节 `SetValue/GetValue` 切片** 替代 MTE `DataCopy` 行读，向量路径退化。

**处置**：**已回滚**至 `RunShakePrfBatchUbWithUb` + `SamplePolyCbd2Batch8WithUb`（GM 中转）；删除 `RunShakePrfBatchIntoBuf` / `SamplePolyCbd2Batch8FromPrfUb`。

---

## Opt-3 — SHAKE‖rej 双缓冲（2026-06-25）**已关闭**

| 项 | 内容 |
|----|------|
| 状态 | **已测否决 + 代码已回滚**；活跃树**无** `F203_AHAT16_PIPE_SHAKE` |
| SIM prep | **800163** vs 基线 **774335**（**+3.3%**） |
| 备份 | `backup/keygen-opt3-pre_20260625185315/`（仅作历史快照，勿再实现） |
| 结论 | SIM 单 AIV 上无有效流水重叠；**不再采纳** |

---

## Opt-4 — 双 AIV Â（2026-06-25）**✅ 已合入**

**目标**：`F203_AHAT16_BLOCK_DIM=2`，`kPrepBlockDim=2`。

**根因**：内嵌 `RunKernelShakeGeneralUb` 沿用外层 `GetBlockIdx()`；`blockDim=2` 时 block1 上 batch=1 的 SHAKE 空转 → poly 8–15 未写入。修复：`KernelShakeGeneral::ProcessInline()`（`library/shared/shake_xof_kernel/shake_general.h`）。

| 项 | tick / 结果 |
|----|-------------|
| a_hat16 SIM dual | **381544**（vs 单 AIV ~715k，**−46.7%**）；golden **PASS** |
| G4 prep SIM dual | **454170**（vs Opt-2 基线 **774335**，**−41.4%**） |
| G4 全链 SIM dual | **532074**（vs **852305**，**−37.6%**）；compute 77904 不变 |
| G4 CPU+SIM dual | **PASS** |

**处置**：生产默认 **`F203_AHAT16_BLOCK_DIM=2`**（keygen / a_hat16 / prep cmake）；单 AIV 对照须显式 `=1`。

---

## 夜间移交 — exp KeyGen + 技术总结（2026-06-25）

- 定稿：`docs/notes/F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md`
- 新建：`examples/incubating/exp-fips203-mlkem-pke-keygen-k4/`（customspec PDF + 全链 run.sh + prep 内核）
- Opt-5：**暂缓**（用户确认）
- 待明早验收：`cd examples/incubating/exp-fips203-mlkem-pke-keygen-k4 && bash run.sh -r cpu|sim -v Ascend910B4`

---

## Opt-5 — Pipe 细同步（2026-06-26）**✅ 部分合入**

**方案**：[`PIPE_SYNC_EVAL.md`](../../ascendc-tests/pass-fix-f203-alg13-device-keygen-k4/PIPE_SYNC_EVAL.md) Phase 1→5 逐轮实验。

| Phase | 结论 | prep tick（SIM） |
|-------|------|------------------|
| **1** CBD MTE2/V | **合入** | 451813（−0.5% vs 454170） |
| **2** PRF 窄化 | **回滚**（tick 469138，劣于 Phase1） | — |
| **3** Â GM 窄化 | **回滚**（联测仍慢） | — |
| **4** Alg7 d12/rej | **回滚**（`a_hat max_abs_diff≈3272`） | — |
| **5** C-04 删减 | **合入**；P-03 删减 PASS 但 tick 449039 → **保留 P-03 ALL** | **447061（−1.6%）** |

| 指标 | Opt-5 后 | Opt-4 基线 |
|------|----------|------------|
| G4 prep SIM | **447061** | 454170 |
| G4 全链 SIM | **524986** | 532074 |
| G4 CPU+SIM | **PASS** | PASS |

**合入代码**：`f203_cbd_eta2_ub_io.hpp`（CopyIn→MTE2、Vector 前→V；CopyOut 后无 barrier）；P-02/P-04 仍为 `PIPE_ALL`；PRF/Â/Alg7 保持 `PIPE_ALL`。
**exp 交付**：`examples/incubating/exp-fips203-mlkem-pke-keygen-k4/`（经 `ALG8_INC` 自动继承 CBD 窄化）。
