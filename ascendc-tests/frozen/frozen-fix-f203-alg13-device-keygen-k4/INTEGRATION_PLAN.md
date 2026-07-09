# INTEGRATION_PLAN — Alg.13 设备全链 KeyGen（k=4）

**探针**：`pass-fix-f203-alg13-device-keygen-k4`（**pass-** 前缀：CPU+SIM 生产 I/O 已验收）  
**状态**：生产单入口 `ascendc_keygen_bbit` ✅；prep 性能优化按 §1.1 顺序 **2134** 待实验  
**实现方案**：[`pass-fix-f203-alg13-device-keygen-k4-实现方案-customspec.tex`](pass-fix-f203-alg13-device-keygen-k4-实现方案-customspec.tex)
**讨论**：[`qa/2026-06/2026-06-25-KeyGen-prep优化路线图.md`](../../qa/2026-06/2026-06-25-KeyGen-prep优化路线图.md)（Step4 单 TPipe · prep 优化实验顺序 **2134**）  
**上游模块**（禁止从 `frozen/` 抄码）：

| 行 | 模块 |
|----|------|
| 1, 3–7 | [`pass-fix-f203-alg7-sample-ntt-k4`](../pass-fix-f203-alg7-sample-ntt-k4/)、[`pass-fix-f203-alg13-lines3-7-a-hat-k4`](../pass-fix-f203-alg13-lines3-7-a-hat-k4/) |
| 8–15 | [`pass-fix-f203-alg13-lines8-15-se-k4`](../pass-fix-f203-alg13-lines8-15-se-k4/) |
| 16–21 | 本目录 `compute/`（vendored vec-k4-v2）+ **ek‖ρ** |

**技术总结**：[`docs/notes/F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md`](../../docs/notes/F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md)、[`docs/notes/F203-2s1e-NTT内积UB融合技术总结.md`](../../docs/notes/F203-2s1e-NTT内积UB融合技术总结.md)、[`docs/notes/F203-Alg7-SampleNTT-单poly技术总结.md`](../../docs/notes/F203-Alg7-SampleNTT-单poly技术总结.md)

---

## 0. 目标

**单次 KeyGen 语义**（FIPS 203 Alg.13，ML-KEM-768 / k=4）：

```text
SEED_D → G(d‖k) → (ρ, σ)
  → 行 3–7：16× SampleNTT(ρ) → a_hat[16,256]
  → 行 8–15：PRF(σ)+CBD → src[8,256]
  → 行 16–20：NTT → Â·ŝ+ê → ByteEncode₁₂ → ek_polyvec[1536], sk_polyvec[1536]
  → 行 21：ek_PKE = ek_polyvec ‖ ρ[32]   (1568 B)
           dk_PKE = sk_polyvec            (1536 B)
```

**验收**：生产 I/O 下 `ek_pke`/`dk_pke` 字节一致（`KEYGEN_VERIFY=1` 或 liboqs KAT）；分段 golden 仍可用 `scripts/gen_data.py`（**禁止**作为默认 `input/` 契约）。

**生产 I/O 契约**（与 `exp-fips203-mlkem-pke-keygen-k4` customspec 一致）：

```text
input/  — seed_d.bin + lut_even/odd_stacked.bin
output/ — ek_pke.bin + dk_pke.bin
Host：prepare_production_input → ascendc_keygen_bbit（2 launch，GM 不落中间盘）
```

**非目标（本阶段）**：单 kernel 融合 Keccak+MIX；FIPS 字面 32B `d`（沿用探针 `DerandFromSeedD`）；`examples/stable` 晋级。

---

## 1. 架构：G4 固定 2 Launch（硬约束）

**硬约束**（G4 生产编排）：

1. **最多 2 次 device kernel launch**：Launch 1 prep（行 3–15）+ Launch 2 compute（行 18–21）。
2. **行 21 `ek_PKE = ek_polyvec ‖ ρ` 必须在 Launch 2（vec-k4-v2）内核内融合**（`F203_KEYGEN_EK_PKE=1`，`FuseEkPke`）。  
   **禁止**为简单 GM 拼接单独再起 launch（含 `f203_keygen_ek_append`、Host `host_ek_append.py` 接入 G4）。
3. **G1**（`f203_keygen_ek_append` 孤立核）仅作历史回归门禁，**不得**接入 G4 编排。

**终态目标**：

```text
Launch 1  行 3–15   f203_keygen_prep        → a_hat, src, ρ GM
Launch 2  行 16–21  vec-k4-v2 mixPass=0     → S1→S2 MMAD→S3→内积→Encode + ek_PKE（ek‖ρ 内核融合）
```

**缩减路线**（每步验收后再下一步）：

| 步骤 | G4 device Launch | 变更 |
|------|------------------|------|
| 基线 | 4 | B Â + C presample + D vec-v2 + E ek_append |
| Step1 ✅ | 3 | 独立 `f203_keygen_ek_append`（已废止于 G4） |
| Step2 ✅ | 3 / 2 编排段 | `run_chain_compute` 对照 |
| Step3 ✅ | 3 / 2 编排段 | `run_prep` legacy 子探针 |
| **Step4 ✅** | **2** | prep + vec **单 launch**（mixPass=4 + ek‖ρ 融合） |

当前 G4 编排（Step4，**生产 run.sh**）：

```text
Host  prepare_production_input → ascendc_keygen_bbit
Launch 1  行 3–15  f203_keygen_prep                         → a_hat, src, ρ（GM，不写盘）
Launch 2  行 16–21 mmad_custom（mixPass=0，F203_KEYGEN_EK_PKE=1）→ ek_pke, dk_pke → output/
```

**禁止**：默认 run 向 `input/` 写入 `a_hat/src/rho` 或向 `output/` 写入中间 golden；`compute_io/` 磁盘 staging 非生产接口。

**SIM**：全链 `bash run.sh -r sim`；核占用见 `sim_log/profile_*.toml`（勿用 CPU SUCCESS 数 AI Core）。

**性能基线（生产单入口，2026-06-28 SIM）**：Total tick **886801**（prep task0 ~806k + mmad task1 ~81k）。历史分段 prep+compute 见 [`qa/2026-06/2026-06-28-KeyGen探针pass前缀与生产IO.md`](../../qa/2026-06/2026-06-28-KeyGen探针pass前缀与生产IO.md)。

---

## 1.1 prep 性能优化路线图（待实验）

**原则**（用户 2026-06-25 拍板）：**一轮一实验**；CPU+SIM G4 PASS 后记录 prep tick；**有效则叠加下一项**，**无效则回滚**，不静默保留负优化。

**实验顺序**：**Opt-4 ✅**；**5** 暂缓。

| 编号 | 技术点 | 状态 | 说明 | 预期 / 已知 |
|------|--------|------|------|-------------|
| **—** | Step4 单 TPipe + 一次 G（`f203_keygen_prep_ub.hpp`） | **✅ 已合入** | 相对 Step3 分段 prep 之和 ~809521 **−4.3%**（774357） | 多 TPipe 硬拼曾 **+3.7%**（839178），已回滚 |
| **2** | 子探针统一一次 G 派生 ρ‖σ | **✅ 2026-06-25** | `f203_alg7_g.hpp::BuildRhoSigmaFromSeedD`；presample 经 `f203_se_vector_g.hpp` 委托 | 分段之和 809521→**808921**；G4 prep **774335**（≈不变） |
| **1** | PRF → CBD **跳过 `prf_out` GM** | **❌ 已测否决** | SIM prep **822874**（+6.3%） | 已回滚 |
| **3** | Â **16 poly 流水**（SHAKE ‖ rej 双缓冲） | **已关闭** | SIM prep **+3.3%**；代码已回滚，无 `PIPE_SHAKE` 开关 | 见 qa §Opt-3 |
| **4** | 双 AIV Â（`F203_AHAT16_BLOCK_DIM=2`） | **✅ 已合入** | SHAKE `ProcessInline` 修 SIM 分片；prep **454170**（−41.4%） | 默认 **2** |
| **5** | `PipeBarrier<PIPE_ALL>` → MTE↔V 细同步 | **✅ 部分合入** | prep **447061**（−1.6% vs Opt-4）；Phase 2–4 回滚 | 见 [`PIPE_SYNC_EVAL.md`](PIPE_SYNC_EVAL.md) §11 |

**明确不做（本阶段主线）**：

| 项 | 原因 |
|----|------|
| `F203_AHAT16_BATCH_SHAKE=1` | a_hat 探针已证 **+31%** tick（960098 vs 734k） |
| prep + compute **1 launch** | compute 仅 ~78k tick；省 launch 有限；要赚须 a_hat/src 不落 GM 进 NTT，≈ 重写 vec-k4-v2 |
| 504B XOF 默认切换 | 与当前 672B 锁定语义不同，须单独实验门控 |

**每项实验验收**（与 §3 G4 相同 + tick 记录）：

```bash
cd ascendc-tests/pass-fix-f203-alg13-device-keygen-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
# 记录 [keygen] segment prep (行3-15) sum=… 与 STATUS 表
```

讨论纪要：[`qa/2026-06/2026-06-25-KeyGen-prep优化路线图.md`](../../qa/2026-06/2026-06-25-KeyGen-prep优化路线图.md)。

---

## 2. I/O 契约

### 2.1 生产（默认 `run.sh`）

| 路径 | 形状 | 说明 |
|------|------|------|
| `input/seed_d.bin` | uint32 LE | 唯一随机源（`prepare_production_input.py`） |
| `input/lut_even_stacked.bin` | 同 vec-k4-v2 | compute LUT（Host 写入 `ws` GM） |
| `input/lut_odd_stacked.bin` | 同上 | 同上 |
| `output/ek_pke.bin` | **1568 B** | 行 21：`ek_polyvec ‖ ρ` |
| `output/dk_pke.bin` | **1536 B** | 行 20：`sk_polyvec` |

中间量（`a_hat`、`src`、`rho`、`prf_out` 等）**仅 GM 传递**，默认**不写盘**。`KEYGEN_DEBUG_DUMP=1` → `output/debug/`。

### 2.2 调试 golden（`scripts/gen_data.py` / `KEYGEN_VERIFY=1`）

| 路径 | 形状 | 说明 |
|------|------|------|
| `output/golden_*.bin` | 各段 | Host `keygen_golden.py` 生成；**非**默认 `input/` 契约 |
| G0–G4 分段 | 见 §3 | 须显式 `KEYGEN_GATE=g*`；磁盘 staging 仅调试 |

---

## 3. 分门禁 G0–G4

| 门禁 | 内容 | 通过标准 |
|------|------|----------|
| **G0** | Host `scripts/keygen_golden.py` + `gen_data.py` | 全 golden 自洽；`ek_pke == ek_polyvec‖rho` |
| **G1** | **历史回归**：孤立 `f203_keygen_ek_append`（**禁止 G4 编排**） | CPU/SIM 对拍 `golden_ek_pke.bin` |
| **G2** | 编排 presample V3，`SEED_D` 一致 | `src` vs `golden_src` |
| **G3** | 编排 16-poly Â，`SEED_D` 一致 | `a_hat` + `rho` vs golden |
| **G4** | prep + vec 单 launch（行 18–21 + **ek‖ρ 内核融合**） | `ek_pke`/`dk_pke`/`dst` 等 vs golden；**2 launch** |

```bash
cd ascendc-tests/pass-fix-f203-alg13-device-keygen-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4

# 分段门禁（调试，须显式 KEYGEN_GATE）
KEYGEN_GATE=g0 bash run.sh
KEYGEN_GATE=g1 bash run.sh -r cpu -v Ascend910B4
KEYGEN_GATE=g2 bash run.sh -r cpu -v Ascend910B4
KEYGEN_GATE=g3 bash run.sh -r cpu -v Ascend910B4
```

---

## 4. 模块复用与禁止项

| 允许 | 禁止 |
|------|------|
| 子探针 `run.sh` + `cp` GM 产物到本目录 `input/`/`output/` | 从 `frozen/` 抄实现 |
| vec-k4-v2 的 C ref（`hat_inner_product_ref.c`、`byte_encode12_ref.c`）作 golden | 改已锁定 `SEED_D` derand 串（未经确认） |
| `f203_se_device_keccak.hpp` 标量 G（第三方 SHA3 到位前） | 单 launch 强行融合 Â+SE+NTT |

---

## 5. 风险

| 风险 | 对策 |
|------|------|
| vec-v2 `gen_data` 原用 `FIXED_POLY`/`SEED_AHAT` | G4 编排须 `KEYGEN_ORCHESTRATE=1`（vec-v2 `run.sh` 跳过 `gen_data`，保留上游 `a_hat`/`src`） |
| SIM 多 launch 墙钟 | `KEYGEN_KERNEL_BUDGET_SEC` 默认 **900**（勿沿用子探针残留的 60/120） |
| ρ 双份计算漂移 | G3 输出 `rho.bin`；G1/G4 **只读 GM ρ**，不在 2s1e 内重算 G |

---

## 6. 编译与运行选项

**默认 = 生产全链**（Â scalar-rej + presample **V3** + vec-k4-v2 全量向量路径 + ek‖ρ）；**无需**手动 export `HAT_*` / `SE_VECTOR_STAGE` 等。

详见 **[`BUILD_OPTIONS.md`](BUILD_OPTIONS.md)**（含子探针 CMake 表、标准验收命令、调试对照示例）。

`run.sh` 编排子探针时通过 `_keygen_prod_env` **锁定**上述默认，避免 shell 残留 debug env。
