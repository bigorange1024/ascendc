# 2026-06-24 — Alg.7 单 poly 验收结案 / R5 向量 compact / API 查阅 Rule

---

## 1. 单 poly 能否称「写好」

**结论（用户确认口径）**：**可以**——在 **I/O 等价 / 工程 POC** 意义下，单 `(j,i)` 全链 **SHAKE → d1/d2 → â[256]** 已 **CPU+SIM PASS**（`F203_ALG7_REJ_IMPL=1` 默认）。

| 算 | 不算 |
|----|------|
| `â[256]` 与 golden 一致 | FIPS 字面 `while` lazy tail |
| R0–R4 门禁过 | 16×4 `Â` batch |
| 672B 固定 XOF 工程决策 | 全向量热路径（compact 仍标量） |

定稿：[`docs/notes/F203-Alg7-SampleNTT-单poly技术总结.md`](../../docs/notes/F203-Alg7-SampleNTT-单poly技术总结.md)

---

## 2. R5 向量 compact 最后一轮（结案）

**目标**：8-lane LUT + `Gather` 替代 compact 标量 `GetValue`。

| 轮次 | API | SIM |
|------|-----|-----|
| 1 | `Compares(EQ)` int32 + bit 取反 | ❌ `a_hat` 全 0 |
| 2 | `Compares(LT)` int32（**违规**，未 Cast） | ❌ 同上 |
| 3 | `Cast→float` + `Compares(LT)` | ❌ 同上 |
| 4 | `Compare(t,u,EQ)` tensor | ❌ 同上 |

**Host Python**：LUT 语义与 golden **一致**（`lsb` + EQ 取反）。

**决议**：**回退标量 compact**；`f203_alg7_rej_compact.hpp` 保留草稿；R5 标 **理论可行、工程阻塞**（SIM 掩码读法）。

---

## 3. AscendC API 查阅 Rule（用户要求）

已写入 [`.cursor/rules/ascendc-development.mdc`](../../.cursor/rules/ascendc-development.mdc) **「AscendC API 查阅（强制）」**：

1. 写码前查 `library/documents/CANN-AscendC算子开发接口参考-查阅索引.md`  
2. 无记录则查 CANN 9.0 PDF  
3. 同一轮写回索引  

索引已追加 §2026-06-23：`Compares` / `Compare` / `GetCmpMask` / `GatherMask` / `Select`（A2 int32 仅 EQ、bit 打包等）。

**教训**：R5 首轮失败因 **未按索引查 API** 即用 `Compares(NE/LT)` on int32。

---

## 4. 性能 ~80k tick（用户纠结）

**实测**（全链，672B，标量 compact，SIM）：**~80100** tick（较 STATUS 中 rej-only ~63222 高约 17k，因 **全链合一** + Init/GM 等）。

| 因素 | 量级 |
|------|------|
| 672B vs 504B | **+~13%**（~7.5k） |
| SHAKE | 主导（Host profile XOF ~95%） |
| rej 向量 vs 标量 | **可忽略**（<0.1%） |
| 向量 compact（未接通） | 无影响 |

**不大改的微优化候选项**：rej 去重 `DataCopy`、ROM `DataCopy` 替代 448×`SetValue`、仅写 `â` GM——期望有限；要明显降 tick 需 **504B+tail** 或 batch 摊薄。

---

## 5. 生产默认与遗留

| 项 | 状态 |
|----|------|
| `F203_ALG7_REJ_IMPL=1` | 生产默认 |
| compact | **标量** |
| `F203_ALG7_D12_GATHER=0` | 解交织标量（Gather 实验负优化） |
| R5 向量 compact | **暂停**；建议独立探针 |
| 母探针 16-poly | 未开工；可迁入本探针模块 |

**验收命令**：见 [`STATUS.md`](../../ascendc-tests/pass-fix-f203-alg7-sample-ntt-k4/STATUS.md)

---

## 6. 文档刷新（本日）

| 路径 | 动作 |
|------|------|
| `docs/notes/F203-Alg7-SampleNTT-单poly技术总结.md` | **新建**定稿 |
| `docs/notes/F203-Alg7-PhaseA-向量化技术总结.md` | 保留；指向单 poly 总结 |
| `qa/TODO.md` | T13c 更新、T13d 遗留 |
| 探针 `STATUS.md` / `INTEGRATION_PLAN.md` §5 | R5 状态、~80k tick |

---

## 7. Alg.13 行 3–7 十六 poly 向量实现（追加）

**探针**：`ascendc-tests/pass-fix-f203-alg13-lines3-7-a-hat-k4/`

| 决策 | 结论 |
|------|------|
| 路径 | 跳过标量，直接向量 d12/rej（`F203_ALG7_REJ_IMPL=1`） |
| SHAKE | **逐 poly** `RunShake128SampleNttUb`（**默认**）；batch16 + `kShakeMsgStride=64` 可跑但 tick +31% |
| UB 全链 | xof/d1/d2 不落 GM；每 poly 链末 `DataCopy` → `a_hat[(p*K+j)*N+c]` |
| blockDim | 默认 1 |

**验收（2026-06-24）**：CPU + `SIM_DIRECT=1` SIM 均 `a_hat PASS max_abs_diff=0`；SIM **733859** tick（墙钟 ~333s）。单 poly 基线 ~80100 tick → 约 **9.2×**（非线性，含固定开销）。

**遗留**：batch16 负优化；504 路径 168-pair 向量 rej 待修；504 升默认须拍板 tail 语义；可选 2 AIV（收益有限）。

### §7.1 batch16 SHAKE 增量（追加）

| 项 | 结果 |
|----|------|
| 根因 | x 消息行 stride=34B → `KernelShakeGeneral` 内 `uint64` 读未对齐 → SIM LSU crash |
| 修法 | `kShakeMsgStride=CeilAlign32(34)=64`，tiling `maxMsgLen=64`，`lengths` 仍 34 |
| 开关 | `F203_AHAT16_BATCH_SHAKE`（CMake CACHE）；**默认 0** |
| SIM 逐条 | **733859** tick（基线，保留默认） |
| SIM batch16 | **960098** tick（PASS 但 +31%）→ **回退默认** |

### §7.2 双 AIV 8+8（追加）

| 项 | 结果 |
|----|------|
| 分片 | blockIdx 0→poly 0–7；1→8–15；GM `a_hat_offset` 无重叠 |
| CPU | tikicpu `GetBlockIdx` 不可靠 → block0 内串行两分片 |
| SIM 1 AIV（默认） | **733859** tick / ~333s |
| SIM 2 AIV | **714150** tick（**−2.7%**）/ ~334s **墙钟持平** |

**tick vs 墙钟（勿误读）**

- **设计**：2 AIV 不减总 Keccak 次数（仍 16×），只把 8+8 分到两核；真机并行时应缩短 **makespan**（理想 ~2× 墙钟）。
- **tick**：714150 ≈ 2×(733859/2) → SIM tick 更像 **各核周期累加**，不是整 launch 关键路径；故 −2.7% **不代表**并行无效。
- **墙钟**：333s→334s 无下降 → WSL `SIM_DIRECT=1` CAModel **未量出**并行加速（长任务 + 仿真开销）；**不能**推断真机无收益，也**不能**当已验证快一倍。
- **对比 504B**：504 减 squeeze → tick **与**墙钟均降；2 AIV 只分片 → 收益应看 makespan，本次 SIM 两指标均未体现 → **保持可选非默认**。
- **详表**：[`INTEGRATION_PLAN` §5.1](../../ascendc-tests/pass-fix-f203-alg13-lines3-7-a-hat-k4/INTEGRATION_PLAN.md)；tick≠墙钟见 [`F203-2s1e-NTT内积UB融合技术总结.md`](../../docs/notes/F203-2s1e-NTT内积UB融合技术总结.md)。

### §7.3 504B XOF 对照（追加）

| 项 | 结果 |
|----|------|
| 开关 | `F203_ALG7_XOF_504=1`（`alg7_geom` / `f203_alg7_layout.h` / ROM / `gen_data` 同步） |
| SIM 设备宏 | `cmake/npu_lib.cmake` → `ascendc_compile_definitions`（否则 SIM 仍按 672 编译、对拍失败） |
| 672B 1 AIV（默认） | **733859** tick / ~333s |
| **504B 1 AIV** | **549224** tick（**−25.2%**）/ ~263s（**−21%**） |
| rej | 504 路径暂 **标量 rej**（168-pair 向量 interleave 待修） |
| 对拍 | `SEED_D=20260619` 与 672 golden **bit-identical** |
| 解读 | 单 poly 672→504 约 +13%；16 poly **−25%** → Keccak 占比更高 |
| 默认 | 仍为 **672B** + vec rej（squeeze 策略冻结；504 为实验对照，见 [`INTEGRATION_PLAN` §5.1](../../ascendc-tests/pass-fix-f203-alg13-lines3-7-a-hat-k4/INTEGRATION_PLAN.md)） |

---

## 8. Alg.13 全链 KeyGen（`pass-fix-f203-alg13-device-keygen-k4`）

**目标**：G0–G4 门禁 — Host golden → ek‖ρ 核 → presample → 16-poly Â → vec-k4-v2 → 全链。

| 门禁 | CPU（2026-06-24） | 说明 |
|------|-------------------|------|
| G0 | ✅ | `keygen_golden.py` 自洽 |
| G1 | ✅ | `f203_keygen_ek_append` |
| G2 | ⚠️ | presample `.so` 缺失 → `golden_src` fallback，对拍仍 PASS |
| G3 | ✅ | 编排 Â |
| G4 | ✅ | ✅ | 全链 CPU ~91s / SIM ~504s |

**阻塞修**：G4 CPU 曾 `ek_pke` 失败 — `vec-v2 run.sh` 内 `gen_data` 用 `SEED_AHAT` 随机 `a_hat` 覆盖编排输入；修法 **`KEYGEN_ORCHESTRATE=1`**（vec-v2 跳过 gen_data，仅 patch tiling mixPass 5→4）。

**SIM**：`KEYGEN_KERNEL_BUDGET_SEC=900`；presample V4 产出≠golden → **src fallback**；G4 SIM ✅ ~504s。

**ρ**：G3/G4 暂 Host `golden_rho` 作 `rho.bin`（Â 探针未 dump 设备 ρ）；G1 只读 GM 拼接。

探针：[`INTEGRATION_PLAN`](../../ascendc-tests/pass-fix-f203-alg13-device-keygen-k4/INTEGRATION_PLAN.md)、[`STATUS`](../../ascendc-tests/pass-fix-f203-alg13-device-keygen-k4/STATUS.md)。
